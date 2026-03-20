#include <tpactor/actors/timer_service.hpp>
#include <tpactor/core/registry.hpp>

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

	tpaactors::TimerService& service = [&] {
		while (true) {
			auto service_opt = registry.find_actor<tpaactors::TimerService>(argv[1]);
			if (service_opt) {
				return service_opt.value();
			}
			// Wait for the service to be created
		}
	}();

	tpaactors::TimerServiceRequester requester{"Requester", service};
	requester.do_registrations(registry);

	std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();

	for (int batch_idx = 0; batch_idx < 10; ++batch_idx) {
		std::map<std::chrono::steady_clock::time_point, tpaactors::TimerCallback> batch;
		for (int i = 0; i < 5; ++i) {
			std::chrono::steady_clock::time_point const target_time = start_time + std::chrono::seconds(batch_idx + 2 * i);
			batch.emplace(target_time, [batch_idx, i] { std::cout << "Batch " << batch_idx << " callback " << i << " called!" << std::endl; });	
		}

		requester.request_timeout_batch(std::move(batch));
	}

	while (true) {
		requester.poll();
	}

	return 0;
}