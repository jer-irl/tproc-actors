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

    example::PongActor pong{"PongActor"};
    pong.do_registrations(registry);
    
    std::cout << "PongActor created waiting to bind PingActor..." << std::endl;

    while (!registry.find_actor<example::PingActor>("PingActor")) {
        // Wait for PingActor to be created
    }

    example::PingActor& ping = registry.find_actor<example::PingActor>("PingActor")->get();
    std::cout << "PingActor found!" << std::endl;
    pong.set_ping_actor(ping);

    while (!pong.got_ping()) {
        pong.poll();
    }

    registry.stop();
    registry_worker.join();

    std::cout << "all done!" << std::endl;

    return 0;
}
