#include <tpactor/actors/seqr.hpp>
#include <tpactor/core/registry.hpp>

#include <thread>

namespace tpacore = tpactor::core;
namespace tpaactors = tpactor::actors;

int main(int argc, char *argv[]) {
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <seqr_name>" << std::endl;
		return 1;
	}

	tpacore::ActorRegistry& registry = tpacore::ActorRegistry::instance()->get();
	auto registry_worker = std::thread{
		[&registry] {
			registry.run();
		}
	};

	auto& seqr = [&registry, &argv] -> tpaactors::Sequencer<std::string>& {
		while (true) {
			auto seqr_opt = registry.find_actor<tpaactors::Sequencer<std::string>>(argv[1]);
			if (seqr_opt) {
				return seqr_opt.value();
			}
			// Wait for the sequencer to be created
		}
	}();

	std::cout << "Sequencer found" << std::endl;

	while (seqr.begin() == seqr.end()) {
		// Wait for the first message
	}
	auto it = seqr.begin();
	while (true) {
		std::cout << "Got string: " << **it << std::endl;
		while (true) {
			auto next = it;
			++next;
			if (next != seqr.end()) {
				break;
			}
			// Wait for the next message
		}
		
		++it;
	}

	registry.stop();
	registry_worker.join();

	return 0;
}