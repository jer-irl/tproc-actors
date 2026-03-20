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

	tpaactors::Sequencer<std::string> sequencer{argv[1]};
	sequencer.do_registrations(registry);

	while (true) {
		sequencer.poll();
	}

	return 0;
}
