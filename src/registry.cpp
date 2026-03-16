#include "tpactor/registry.hpp"
#include "tpactor/rings.hpp"

#include <tproc.h>

#include <atomic>
#include <stdexcept>

namespace tpactor {

struct alignas(64) RegistryRingEntry {
	enum class Kind {
		Register,
		Unregister
	};
	Kind kind;
	std::array<char, 32> name;
	Actor* actor;
};

using RegistryRing = MpscRing<RegistryRingEntry, 128>;

struct GlobalShmScratch {
	std::atomic_uint64_t num_tprocs;
	std::array<RegistryRing, 128> registry_rings;
};

Registry::Registry() {
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


} // namespace tpactor
