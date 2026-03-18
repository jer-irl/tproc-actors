#include "tpactor/actor.hpp"
#include "tpactor/registry.hpp"

#include <atomic>
#include <iostream>
#include <thread>

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

class PingActor : public tpactor::Actor<PingActor, tpactor::Receivables<NoisyInt>, tpactor::Destructables<NoisyInt>> {
public:
    using Actor::Actor;
    
    auto set_pong_actor(PongActor& pong_actor) {
        pong_actor_ = &pong_actor;
        ready_.store(true, std::memory_order_release);
    }

    auto send_ping(int val) ->void;

    auto on_message_impl(tpactor::UniqueResourcePtr<NoisyInt> message) -> void {
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

class PongActor : public tpactor::Actor<PongActor, tpactor::Receivables<NoisyInt>, tpactor::Destructables<NoisyInt>> {
public:
    using Actor::Actor;

    auto set_ping_actor(PingActor& ping_actor) {
        ping_actor_ = &ping_actor;
        ready_.store(true, std::memory_order_release);
    }

    auto on_message_impl(tpactor::UniqueResourcePtr<NoisyInt> message) -> void {
        std::cout << "Got ping with value " << message->val << std::endl;
        ping_actor_->send_to(tpactor::UniqueResourcePtr{new NoisyInt(message->val + 1), tproc_id()});
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

auto PingActor::send_ping(int val) -> void {
    std::cout << "Sending ping with value " << val << std::endl;
    pong_actor_->send_to(tpactor::UniqueResourcePtr{new NoisyInt{val}, tproc_id()});
    std::cout << "Ping sent" << std::endl;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " [PING|PONG]" << std::endl;
        return 1;
    }

    std::string dummy;

    tpactor::ActorRegistry& registry = tpactor::ActorRegistry::instance()->get();

    auto registry_worker = std::thread{
        [&registry] {
            registry.run();
        }
    };

    if (std::string_view(argv[1]) == "PING") {
        PingActor ping{"PingActor"};

        std::cout << "PingActor created waiting to bind PongActor..." << std::endl;

        while (!registry.find_actor<PongActor>("PongActor")) {
            // Wait for PongActor to be created
        }

        PongActor& pong = registry.find_actor<PongActor>("PongActor")->get();
        std::cout << "PongActor found!" << std::endl;
        ping.set_pong_actor(pong);

        while (!ping.ready() || !pong.ready()) {
            // Wait for both actors to be ready
        }

        ping.send_ping(123);

        while (!ping.got_pong()) {
            ping.poll();
        }

    } else if (std::string_view(argv[1]) == "PONG") {
        PongActor pong{"PongActor"};
        std::cout << "PongActor created waiting to bind PingActor..." << std::endl;

        while (!registry.find_actor<PingActor>("PingActor")) {
            // Wait for PingActor to be created
        }

        PingActor& ping = registry.find_actor<PingActor>("PingActor")->get();
        std::cout << "PingActor found!" << std::endl;
        pong.set_ping_actor(ping);

        while (!pong.got_ping()) {
            pong.poll();
        }

    } else {
        std::cout << "Invalid argument: " << argv[1] << std::endl;
        return 1;
    }

    registry.stop();
    registry_worker.join();

    std::cout << "all done!" << std::endl;

    return 0;
}
