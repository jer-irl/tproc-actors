#pragma once

#include <tpactor/core/actor.hpp>
#include <tpactor/core/registry.hpp>

#include <atomic>
#include <iostream>

namespace example {

namespace tpacore = tpactor::core;

struct NoisyInt {
    NoisyInt(int val) : val(val) {
        std::cout << "Constructing NoisyInt with value " << val << std::endl;
    }
    ~NoisyInt() {
        std::cout << "Destructing NoisyInt with value " << val << std::endl;
    }

    NoisyInt(const NoisyInt& other) = delete;
    NoisyInt& operator=(const NoisyInt& other) = delete;
    NoisyInt(NoisyInt&& other) noexcept = delete;
    NoisyInt& operator=(NoisyInt&& other) noexcept = delete;

    operator int() const {
        return val;
    }
    int val;
};

class PongActor;

class PingActor : public tpacore::Actor {
public:
    using Receivables = std::tuple<NoisyInt>;
    using Destructables = std::tuple<NoisyInt>;
    
    using Actor::Actor;

    auto set_pong_actor(PongActor& pong_actor) {
        pong_actor_ = &pong_actor;
        ready_.store(true, std::memory_order_release);
    }

    auto send_ping(int val) ->void;

    auto on_message_impl(tpacore::UniqueResourcePtr<NoisyInt const> message) -> void {
        std::cout << "Got pong with value " << message->val << std::endl;
        got_pong_ = true;
    }

    auto ready() const {
        return ready_.load(std::memory_order_acquire);
    }

    auto got_pong() const {
        return got_pong_;
    }

private:
    PongActor* pong_actor_;
    std::atomic_bool ready_{false};
    bool got_pong_{false};
};

class PongActor : public tpacore::Actor {
public:
    using Receivables = std::tuple<NoisyInt>;
    using Destructables = std::tuple<NoisyInt>;
    
    using Actor::Actor;

    auto set_ping_actor(PingActor& ping_actor) {
        ping_actor_ = &ping_actor;
        ready_.store(true, std::memory_order_release);
    }

    auto on_message_impl(tpacore::UniqueResourcePtr<NoisyInt const> message) -> void {
        std::cout << "Got ping with value " << message->val << std::endl;
        ping_actor_->send_to(tpacore::UniqueResourcePtr{new NoisyInt(message->val + 1), tproc_id()});
        got_ping_ = true;
    }

    auto ready() const {
        return ready_.load(std::memory_order_acquire);
    }

    auto got_ping() const {
        return got_ping_;
    }

private:
    PingActor* ping_actor_;
    std::atomic_bool ready_{false};
    bool got_ping_{false};
};

inline auto PingActor::send_ping(int val) -> void {
    std::cout << "Sending ping with value " << val << std::endl;
    pong_actor_->send_to(tpacore::UniqueResourcePtr{new NoisyInt{val}, tproc_id()});
    std::cout << "Ping sent" << std::endl;
}

} // namespace example
