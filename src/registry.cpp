#include <tpactor/core/registry.hpp>

#include <tpactor/core/mem.hpp>
#include <tpactor/core/rings.hpp>

#include <mutex>
#include <tproc.h>

#include <atomic>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <typeinfo>
#include <variant>

namespace tpactor::core {

struct alignas(64) RegistryRingEntry {
	enum class Kind {
		NewTproc,
		Register,
		Unregister,
		Release,
	};
	Kind kind;
	struct FlatActor {
		std::uint64_t tproc_id;
		std::array<char, 32> name;
		std::type_info const* type_info;
		void* ptr;
	};
	std::variant<std::uint64_t, FlatActor, UniqueResourcePtr<void>> payload;
};

constexpr std::size_t kMaxTprocs = 256;
constexpr std::size_t kRingEntries = 128;

struct RegistryRing : MpscRing<RegistryRingEntry, kRingEntries> {};

struct GlobalShmScratch {
	std::atomic_uint64_t num_tprocs;
	std::array<std::atomic<RegistryRing*>, kMaxTprocs> registry_rings;
};

static_assert(sizeof(GlobalShmScratch) <= 4096,
	"GlobalShmScratch must fit within the shared registry page");

ActorRegistry::ActorRegistry() {
	if (!tproc_is_threadproc()) {
		throw std::runtime_error("Registry can only be used within a threadproc");
	}

	void* shm = tproc_registry();
	if (!shm) {
		throw std::runtime_error("Failed to access threadproc registry");
	}

	shm_scratch_ = reinterpret_cast<GlobalShmScratch*>(shm);

	tproc_id_ = shm_scratch_->num_tprocs.fetch_add(1, std::memory_order_acq_rel);
	std::cout << "ActorRegistry initialized for tproc with id " << tproc_id_ << std::endl;

	if (tproc_id_ >= shm_scratch_->registry_rings.size()) {
		throw std::runtime_error("Too many threadprocs; registry rings exhausted");
	}

	// Allocate this tproc's ring on the heap (visible to all threadprocs
	// via CLONE_VM) and publish the pointer into the shared page.
	auto* ring = new RegistryRing{};
	own_ring_ = ring;
	shm_scratch_->registry_rings[tproc_id_].store(ring, std::memory_order_release);

	// Notify any existing tprocs of our existence by pushing a "new tproc" entry into their rings.  This allows them to clean up any state associated with tprocs that have since exited.
	for (uint64_t i = 0; i < shm_scratch_->num_tprocs.load(std::memory_order_acquire); i++) {
		if (i == tproc_id_) {
			continue; // No need to notify ourselves
		}
		auto* ring = shm_scratch_->registry_rings[i].load(std::memory_order_acquire);
		if (!ring) {
			continue; // tproc claimed a slot but hasn't published its ring yet
		}
		RegistryRingEntry entry{
			.kind = RegistryRingEntry::Kind::NewTproc,
			.payload = tproc_id_,
		};
		while (!ring->push(std::move(entry))) {
			// Wait for space in the ring
		}
	}

	// drain anything that arrived before we claimed our slot
	while (poll_queue()) {}
}

auto ActorRegistry::poll_queue() -> bool {
	RegistryRingEntry entry;
	if (own_ring_->pop(entry)) {
		switch (entry.kind) {
			case RegistryRingEntry::Kind::NewTproc:
				{
					std::cout << "Notified of new tproc with id " << std::get<std::uint64_t>(entry.payload) << std::endl;
					RegistryRing* new_ring{};
					while (!new_ring) {
						new_ring = shm_scratch_->registry_rings[std::get<std::uint64_t>(entry.payload)].load(std::memory_order_acquire);
					}
					std::lock_guard lock{actors_mutex_};
					for (auto const& [name, actor_entry] : actors_) {
						if (actor_entry.tproc_id != tproc_id_) {
							continue; // No need to notify the new tproc of actors it registered
						}
						RegistryRingEntry new_actor_entry{
							.kind = RegistryRingEntry::Kind::Register,
							.payload = RegistryRingEntry::FlatActor{
								.tproc_id = tproc_id_,
								.name = {},
								.type_info = actor_entry.type_info,
								.ptr = actor_entry.actor,
							},
						};
						auto& flat_actor = std::get<RegistryRingEntry::FlatActor>(new_actor_entry.payload);
						std::strncpy(flat_actor.name.data(), name.data(), flat_actor.name.size() - 1);
						flat_actor.name[flat_actor.name.size() - 1] = '\0';
						flat_actor.ptr = actor_entry.actor;

						while (!new_ring->push(std::move(new_actor_entry))) {
							// Wait for space in the ring
						}
					}
				}
				break;
			case RegistryRingEntry::Kind::Register:
				{
					std::lock_guard lock{actors_mutex_};
					auto& actor = std::get<RegistryRingEntry::FlatActor>(entry.payload);
					std::cout << "Registering actor: " << std::string_view{actor.name.data(), ::strnlen(actor.name.data(), actor.name.size())} << std::endl;
					actors_.emplace(std::string{actor.name.data()}, ActorEntry{actor.tproc_id, actor.type_info, actor.ptr});
				}
				break;
			case RegistryRingEntry::Kind::Unregister:
				// TODO no alloc
				{
					std::lock_guard lock{actors_mutex_};
					auto& actor = std::get<RegistryRingEntry::FlatActor>(entry.payload);
					actors_.erase(std::string{actor.name.data(), ::strnlen(actor.name.data(), actor.name.size())});
				}
				break;
			case RegistryRingEntry::Kind::Release:
				{
					auto& resource = std::get<UniqueResourcePtr<void>>(entry.payload);
					if (!resource.get()) {
						throw std::runtime_error("Received release request with null resource");
					}
					if (resource.tproc_id_ != tproc_id_) {
						throw std::runtime_error("Received release request for resource not owned by this threadproc");
					}
					auto destructor_it = destructors_.find(resource.type_info_->hash_code());
					if (destructor_it == destructors_.end()) {
						throw std::runtime_error("Received release request for resource with no registered destructor");
					}
					auto& destructor = destructor_it->second;
					destructor(resource.get());
					resource.resource_ = nullptr;
				}
				break;
		}
		return true;
	}
	return false;
}

auto ActorRegistry::register_actor_erased(std::string_view name, std::type_info const& type_info, void* actor) -> std::expected<void, std::string> {
	// TODO ensure does not already exist
	{
		std::lock_guard lock{actors_mutex_};
		actors_.emplace(name, ActorEntry{tproc_id_, &type_info, actor});
	}

	uint64_t num_tprocs = shm_scratch_->num_tprocs.load(std::memory_order_acquire);
	for (uint64_t i = 0; i < num_tprocs; i++) {
		if (i == tproc_id_) {
			continue; // No need to notify ourselves
		}
		auto* ring = shm_scratch_->registry_rings[i].load(std::memory_order_acquire);
		if (!ring) {
			continue; // tproc claimed a slot but hasn't published its ring yet
		}
		RegistryRingEntry entry{
			.kind = RegistryRingEntry::Kind::Register,
			.payload = RegistryRingEntry::FlatActor{
				.tproc_id = tproc_id_,
				.name = {},
				.type_info = &type_info,
				.ptr = actor,
			},
		};
		auto& flat_actor = std::get<RegistryRingEntry::FlatActor>(entry.payload);
		std::strncpy(flat_actor.name.data(), name.data(), flat_actor.name.size() - 1);
		flat_actor.name[flat_actor.name.size() - 1] = '\0';
		flat_actor.ptr = actor;

		while (!ring->push(std::move(entry))) {
			// Wait for space in the ring
		}
	}

	return {};
}

auto ActorRegistry::release_resource(UniqueResourcePtr<void>&& resource) -> void
{
	if (!resource.get()) {
		return;
	}

	auto* ring = shm_scratch_->registry_rings[resource.tproc_id_].load(std::memory_order_acquire);
	if (!ring) {
		return;
	}
	RegistryRingEntry entry{
		.kind = RegistryRingEntry::Kind::Release,
		.payload = std::move(resource),
	};
	while (!ring->push(std::move(entry))) {
		// Wait for space in the ring
	}
}

auto ActorRegistry::num_tprocs() const -> std::uint64_t {
	return shm_scratch_->num_tprocs.load(std::memory_order_acquire);
}

} // namespace tpactor::core
