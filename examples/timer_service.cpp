#include <tpactor/actors/timer_service.hpp>

#include <thread>

namespace tpaactors = tpactor::actors;
namespace tpacore = tpactor::core;

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <service_name>" << std::endl;
		return 1;
	}

	tpacore::ActorRegistry& registry = tpacore::ActorRegistry::instance()->get();
	auto registry_worker = std::thread{
		[&registry] {
			registry.run();
		}
	};

	tpaactors::TimerService timer_service{argv[1]};
	timer_service.do_registrations(registry);

	while (true) {
		timer_service.poll();
		timer_service.check_timeouts();
	}

	return 0;
}
