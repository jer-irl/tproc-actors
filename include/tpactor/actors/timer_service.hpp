#pragma once

#include <tpactor/core/mem.hpp>
#include <tpactor/core/registry.hpp>
#include <tpactor/core/actor.hpp>

#include <chrono>
#include <compare>
#include <list>
#include <map>
#include <queue>

namespace tpactor::actors {

class TimerServiceRequester;
class TimerService;

using TimerCallback = std::function<void()>;

struct TimerServiceRequestBatch {
	std::map<std::chrono::steady_clock::time_point, TimerCallback> callbacks;
	TimerServiceRequester* requester;
};

struct TimerServiceNotification {
	std::chrono::steady_clock::time_point time_target;
	std::chrono::steady_clock::time_point time_actual;
	TimerCallback callback;
};

class TimerServiceRequester : public core::Actor {
public:
	using Receivables = std::tuple<TimerServiceNotification>;
	using Destructables = std::tuple<TimerServiceRequestBatch>;

	explicit TimerServiceRequester(std::string_view name, TimerService& timer_service)
		: Actor{name}, timer_service_(&timer_service)
	{
	}

	auto request_timeout_batch(std::map<std::chrono::steady_clock::time_point, TimerCallback> callbacks) -> void;

	auto on_message_impl(core::UniqueResourcePtr<TimerServiceNotification const> message) -> void {
		std::cout << name() << " got notification for time target "
		          << message->time_target.time_since_epoch().count() << " actual "
		          << message->time_actual.time_since_epoch().count() << std::endl;
		message->callback();
	}

private:
	TimerService* timer_service_;
};

class TimerService : public core::Actor {
public:
	using Receivables = std::tuple<TimerServiceRequestBatch>;
	using Destructables = std::tuple<TimerServiceNotification>;

	using Actor::Actor;

	auto check_timeouts() -> void {
		auto now = std::chrono::steady_clock::now();
		while (!batch_queue_.empty() && batch_queue_.top().target <= now) {
			auto const& record = batch_queue_.top();
			auto const& batch = **record.batch_it;
			auto const& callbacks = batch.callbacks;

			auto const& callback = callbacks.at(record.target);

			batch.requester->send_to(core::UniqueResourcePtr{new TimerServiceNotification{
				.time_target = record.target,
				.time_actual = now,
				.callback = std::move(callback),
			}, tproc_id()});

			if (record.target == callbacks.rbegin()->first) {
				// This was the last callback in the batch, remove it
				batches_.erase(record.batch_it);
				// The UniqueResourcePtr destructor will pass the batch back to the requesting registry, which will free it
			}

			batch_queue_.pop();
		}
	}

	auto on_message_impl(core::UniqueResourcePtr<TimerServiceRequestBatch const> message) -> void {
		std::cout << "Timer service got batch request with " << message->callbacks.size() << " callbacks" << std::endl;
		auto batch_it = batches_.emplace(batches_.end(), std::move(message));
		for (auto& [time_target, callback] : batch_it->get()->callbacks) {
			batch_queue_.emplace(time_target, batch_it);
		}
	}

private:
	using BatchList = std::list<core::UniqueResourcePtr<TimerServiceRequestBatch const>>;
	BatchList batches_;
	struct BatchRecord {
		std::chrono::steady_clock::time_point target;
		BatchList::iterator batch_it;
		friend auto operator<=>(const BatchRecord& lhs, const BatchRecord& rhs) -> std::weak_ordering {
			return lhs.target <=> rhs.target;
		}
	};
	std::priority_queue<BatchRecord, std::vector<BatchRecord>, std::greater<>> batch_queue_;
};

inline auto TimerServiceRequester::request_timeout_batch(std::map<std::chrono::steady_clock::time_point, TimerCallback> callbacks) -> void
{
	TimerServiceRequestBatch batch{
		.callbacks = std::move(callbacks),
		.requester = this,
	};
	timer_service_->send_to(core::UniqueResourcePtr{new TimerServiceRequestBatch(std::move(batch)), tproc_id()});
}

} // namespace tpactor::actors
