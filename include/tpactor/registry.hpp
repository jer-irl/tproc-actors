#pragma once

#include "actor.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace tpactor {

struct GlobalShmScratch;

class Registry {
public:
	static auto instance() -> std::expected<std::reference_wrapper<Registry>, std::string> {
		static Registry registry;
		return registry;
	}

	auto register_actor(std::string_view name, Actor& actor) -> std::expected<void, std::string>;
	auto unregister_actor(std::string_view name) -> std::expected<void, std::string>;

	auto find_actor(std::string_view name) const -> std::expected<std::optional<std::reference_wrapper<Actor>>, std::string>;

private:
	Registry();

	// Return true if a message was dispatched, false if the queue was empty.
	auto poll_queue() -> bool;

	struct ActorEntry {
		std::uint64_t tproc_id;
		Actor* actor;
	};

	std::unordered_map<std::string, ActorEntry> actors_;

	GlobalShmScratch* shm_scratch_;
	std::uint64_t tproc_id_;
};

} // namespace tpactor
