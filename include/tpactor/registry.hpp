#pragma once

#include <tpactor/meta.hpp>

#include <atomic>
#include <cstdint>
#include <expected>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>

namespace tpactor {

struct GlobalShmScratch;
struct RegistryRing;
template <typename T>
class UniqueResourcePtr;

class ActorRegistry {
public:
	static auto instance() -> std::expected<std::reference_wrapper<ActorRegistry>, std::string> {
		static ActorRegistry registry;
		return registry;
	}

	auto stop() {
		should_stop_.store(true, std::memory_order_release);
	}

	auto run() {
		while (!should_stop_.load(std::memory_order_acquire)) {
			poll_queue();
		}
	}

	template <typename ActorT>
	auto register_actor(std::string_view name, ActorT& actor) -> std::expected<void, std::string> 
		requires ActorImpl<std::remove_cvref_t<ActorT>>
	{
		return register_actor_erased(name, typeid(ActorT), static_cast<void*>(&actor));
	}

	template <typename T>
	auto register_destructor(std::function<void(void*)> destructor) -> void {
		destructors_.emplace(typeid(T).hash_code(), std::move(destructor));
	}

	auto unregister_actor(std::string_view name) -> std::expected<void, std::string>;

	enum class FindActorError {
		NotFound,
		TypeMismatch,
	};
	template <typename ActorT>
	auto find_actor(std::string_view name) const -> std::expected<std::reference_wrapper<ActorT>, FindActorError> {
		// TODO no alloc
		std::lock_guard lock{actors_mutex_};
		auto it = actors_.find(std::string{name});
		if (it == actors_.end()) {
			return std::unexpected{FindActorError::NotFound};
		}
		const ActorEntry& entry = it->second;
		if (*entry.type_info != typeid(ActorT)) {
			std::cout << "Type mismatch when finding actor " << name << ": expected " << typeid(ActorT).name() << ", actual " << entry.type_info->name() << std::endl;
			return std::unexpected{FindActorError::TypeMismatch};
		}
		return std::reference_wrapper<ActorT>{*static_cast<ActorT*>(entry.actor)};
	}

	auto release_resource(UniqueResourcePtr<void>&& resource) -> void;

	auto tproc_id() const -> std::uint64_t { return tproc_id_; }

	auto num_tprocs() const -> std::uint64_t;

private:
	ActorRegistry();

	// Return true if a message was dispatched, false if the queue was empty.
	auto poll_queue() -> bool;

	auto register_actor_erased(std::string_view name, std::type_info const& type_info, void* actor) -> std::expected<void, std::string>;

	struct ActorEntry {
		std::uint64_t tproc_id;
		const std::type_info* type_info;
		void* actor;
	};

	mutable std::mutex actors_mutex_;
	std::unordered_map<std::string, ActorEntry> actors_;

	std::unordered_map<std::size_t, std::function<void(void*)>> destructors_;

	GlobalShmScratch* shm_scratch_;
	RegistryRing* own_ring_;
	std::uint64_t tproc_id_;

	std::atomic_bool should_stop_{false};
};

} // namespace tpactor
