#include "ping_pong.hpp"

#include <iostream>
#include <thread>

namespace tpacore = tpactor::core;

int main() {
    tpacore::ActorRegistry& registry = tpacore::ActorRegistry::instance()->get();

    auto registry_worker = std::thread{
        [&registry] {
            registry.run();
        }
    };

    example::PingActor ping{"PingActor"};
    ping.do_registrations(registry);

    std::cout << "PingActor created waiting to bind PongActor..." << std::endl;

    while (!registry.find_actor<example::PongActor>("PongActor")) {
        // Wait for PongActor to be created
    }

    example::PongActor& pong = registry.find_actor<example::PongActor>("PongActor")->get();
    std::cout << "PongActor found!" << std::endl;
    ping.set_pong_actor(pong);

    while (!ping.ready() || !pong.ready()) {
        // Wait for both actors to be ready
    }

    ping.send_ping(123);

    while (!ping.got_pong()) {
        ping.poll();
    }

    registry.stop();
    registry_worker.join();

    std::cout << "all done!" << std::endl;

    return 0;
}
