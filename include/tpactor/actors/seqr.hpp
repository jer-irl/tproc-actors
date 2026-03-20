#pragma once

#include <tpactor/core/actor.hpp>
#include <tpactor/core/mem.hpp>
#include <tpactor/core/tslist.hpp>

namespace tpactor::actors {

template <typename T>
class Sequencer : public core::Actor {
public:
	using Receivables = std::tuple<T>;
	using Destructables = std::tuple<>;

	using Iterator = typename core::SpMoList<core::UniqueResourcePtr<T const>>::Iterator;

	using Actor::Actor;

	auto num_events() const -> std::size_t {
		return events_.size();
	}

	Iterator begin() {
		return events_.begin();
	}
	Iterator end() {
		return events_.end();
	}
	Iterator rbegin() {
		return events_.rbegin();
	}
	Iterator rend() {
		return events_.rend();
	}

	auto on_message_impl(core::UniqueResourcePtr<T const> message) -> void {
		std::cout << "Received message" << std::endl;
		events_.push(std::move(message));
	}

private:
	core::SpMoList<core::UniqueResourcePtr<T const>> events_;
};

} // namespace tpactor::actors
