#pragma once

#include "tpactor/mem.hpp"
#include "tpactor/registry.hpp"
#include <compare>
#include <tpactor/actor.hpp>

#include <chrono>
#include <list>
#include <map>
#include <queue>

namespace example::timingservice {


class TimingServiceRequesterActor;
class TimingServiceActor;

using Callback = std::function<void()>;

struct TimingServiceRequestBatch {
	std::map<std::chrono::steady_clock::time_point, Callback> callbacks;
	TimingServiceRequesterActor* requester;
};

struct TimingServiceNotification {
	std::chrono::steady_clock::time_point time_target;
	std::chrono::steady_clock::time_point time_actual;
	Callback callback;
};

class TimingServiceRequesterActor : public tpactor::Actor {
public:
	using Receivables = std::tuple<TimingServiceNotification>;
	using Destructables = std::tuple<TimingServiceRequestBatch>;

	explicit TimingServiceRequesterActor(std::string_view name, std::string_view service_name)
		: Actor(name), timing_service_(nullptr)
	{
		tpactor::ActorRegistry& registry = tpactor::ActorRegistry::instance()->get();
		while (!timing_service_) {
			auto res = registry.find_actor<TimingServiceActor>(service_name);
			if (res) {
				timing_service_ = &res->get();
			}
			// Wait for the timing service to be registered
		}
		std::cout << "Timing requester " << name << " found timing service " << service_name << std::endl;
	}

	auto request_timeout_batch(std::map<std::chrono::steady_clock::time_point, Callback> callbacks) -> void;

	auto on_message_impl(tpactor::UniqueResourcePtr<TimingServiceNotification> message) -> void {
		std::cout << "Requester " << name() << " got notification for time target "
		          << message->time_target.time_since_epoch().count() << " actual "
		          << message->time_actual.time_since_epoch().count() << std::endl;
		message->callback();
	}

private:
	TimingServiceActor* timing_service_;
};

class TimingServiceActor : public tpactor::Actor {
public:
	using Receivables = std::tuple<TimingServiceRequestBatch>;
	using Destructables = std::tuple<TimingServiceNotification>;

	using Actor::Actor;

	auto check_timeouts() -> void {
		auto now = std::chrono::steady_clock::now();
		while (!batch_queue_.empty() && batch_queue_.top().target <= now) {
			auto& record = batch_queue_.top();
			auto& batch = **record.batch_it;
			auto& callbacks = batch.callbacks;

			auto& callback = callbacks[record.target];

			batch.requester->send_to(tpactor::UniqueResourcePtr{new TimingServiceNotification{
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

	auto on_message_impl(tpactor::UniqueResourcePtr<TimingServiceRequestBatch> message) -> void {
		std::cout << "Timing service got batch request with " << message->callbacks.size() << " callbacks" << std::endl;
		auto batch_it = batches_.emplace(batches_.end(), std::move(message));
		for (auto& [time_target, callback] : batch_it->get()->callbacks) {
			batch_queue_.emplace(time_target, batch_it);
		}
	}

private:
	using BatchList = std::list<tpactor::UniqueResourcePtr<TimingServiceRequestBatch>>;
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

inline auto TimingServiceRequesterActor::request_timeout_batch(std::map<std::chrono::steady_clock::time_point, Callback> callbacks) -> void
{
	TimingServiceRequestBatch batch{
		.callbacks = std::move(callbacks),
		.requester = this,
	};
	timing_service_->send_to(tpactor::UniqueResourcePtr{new TimingServiceRequestBatch(std::move(batch)), tproc_id()});
}

} // namespace example::timingservice
