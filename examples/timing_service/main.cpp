#include "timing_service/timing_service.hpp"

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

	example::timingservice::TimingServiceActor timing_service{argv[1]};
	timing_service.do_registrations(registry);

	while (true) {
		timing_service.poll();
		timing_service.check_timeouts();
	}

	return 0;
}
