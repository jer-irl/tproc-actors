#include "tpactor/registry.hpp"

#include "tpactor/mem.hpp"
#include "tpactor/rings.hpp"

#include <mutex>
#include <tproc.h>

#include <atomic>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <typeinfo>
#include <variant>

namespace tpactor {

struct alignas(64) RegistryRingEntry {
	enum class Kind {
		Register,
		Unregister,
		Release,
	};
	Kind kind;
	struct FlatActor {
		std::array<char, 32> name;
		std::type_info const* type_info;
		void* ptr;
	};
	std::variant<FlatActor, UniqueResourcePtr<void>> payload;
};

using RegistryRing = MpscRing<RegistryRingEntry, 128>;

struct GlobalShmScratch {
	std::atomic_uint64_t num_tprocs;
	std::array<RegistryRing, 128> registry_rings;
};

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

	if (tproc_id_ >= shm_scratch_->registry_rings.size()) {
		throw std::runtime_error("Too many threadprocs; registry rings exhausted");
	}

	// drain
	while (poll_queue()) {}
}

auto ActorRegistry::poll_queue() -> bool {
	auto& ring = shm_scratch_->registry_rings[tproc_id_];
	RegistryRingEntry entry;
	if (ring.pop(entry)) {
		// std::string_view name{entry.actor.name.data(), ::strnlen(entry.actor.name.data(), entry.actor.name.size())};
		switch (entry.kind) {
			case RegistryRingEntry::Kind::Register:
				{
					std::lock_guard lock{actors_mutex_};
					auto& actor = std::get<RegistryRingEntry::FlatActor>(entry.payload);
					std::cout << "Registering actor: " << std::string_view{actor.name.data(), ::strnlen(actor.name.data(), actor.name.size())} << std::endl;
					actors_.emplace(std::string{actor.name.data()}, ActorEntry{tproc_id_, actor.type_info, actor.ptr});
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
						std::runtime_error("Received release request with null resource");
					}
					if (resource.tproc_id_ != tproc_id_) {
						std::runtime_error("Received release request for resource not owned by this threadproc");
					}
					auto destructor_it = destructors_.find(resource.type_info_.hash_code());
					if (destructor_it == destructors_.end()) {
						std::runtime_error("Received release request for resource with no registered destructor");
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
	actors_.emplace(name, ActorEntry{tproc_id_, &type_info, actor});

	uint64_t num_tprocs = shm_scratch_->num_tprocs.load(std::memory_order_acquire);
	for (uint i = 0; i < num_tprocs; i++) {
		if (i == tproc_id_) {
			continue; // No need to notify ourselves
		}
		auto& ring = shm_scratch_->registry_rings[i];
		RegistryRingEntry entry{
			.kind = RegistryRingEntry::Kind::Register,
			.payload = RegistryRingEntry::FlatActor{
				.name = {},
				.type_info = &type_info,
				.ptr = actor,
			},
		};
		auto& flat_actor = std::get<RegistryRingEntry::FlatActor>(entry.payload);
		std::strncpy(flat_actor.name.data(), name.data(), flat_actor.name.size() - 1);
		flat_actor.name[flat_actor.name.size() - 1] = '\0';
		flat_actor.ptr = actor;

		while (!ring.push(std::move(entry))) {
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

	auto& ring = shm_scratch_->registry_rings[resource.tproc_id_];
	RegistryRingEntry entry{
		.kind = RegistryRingEntry::Kind::Release,
		.payload = std::move(resource),
	};
	while (!ring.push(std::move(entry))) {
		// Wait for space in the ring
	}
}

auto ActorRegistry::num_tprocs() const -> std::uint64_t {
	return shm_scratch_->num_tprocs.load(std::memory_order_acquire);
}

} // namespace tpactor
