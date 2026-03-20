#include "tpactor/actors/timer_service.hpp"

#include <thread>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <service_name>" << std::endl;
		return 1;
	}

	tpactor::ActorRegistry& registry = tpactor::ActorRegistry::instance()->get();
	auto registry_worker = std::thread{
		[&registry] {
			registry.run();
		}
	};

	tpactor::actors::TimerServiceActor timer_service{argv[1]};
	timer_service.do_registrations(registry);

	while (true) {
		timer_service.poll();
		timer_service.check_timeouts();
	}

	return 0;
}
