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

	tpactor::actors::TimerServiceRequesterActor requester{"Requester",argv[1]};
	requester.do_registrations(registry);

	std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();

	for (int batch_idx = 0; batch_idx < 10; ++batch_idx) {
		std::map<std::chrono::steady_clock::time_point, tpactor::actors::TimerCallback> batch;
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