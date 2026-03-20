#include "tpactor/core/mem.hpp"
#include <tpactor/actors/seqr.hpp>
#include <tpactor/core/registry.hpp>

#include <thread>

namespace tpacore = tpactor::core;
namespace tpaactors = tpactor::actors;

class StrProducer : public tpacore::Actor {
public:
	using Receivables = std::tuple<>;
	using Destructables = std::tuple<std::string>;

	StrProducer(std::string_view name, tpaactors::Sequencer<std::string>& seqr)
		: Actor{name}, seqr_(&seqr)
	{
	}

	void submit_string(std::string str) {
		std::cout << "Submitting string: " << str << std::endl;
		seqr_->send_to(tpacore::UniqueResourcePtr{new std::string{std::move(str)}, tproc_id()});
	}

private:
	tpaactors::Sequencer<std::string>* seqr_;	
};

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

	tpaactors::Sequencer<std::string>& seqr = [&registry, &argv] {
		while (true) {
			auto seqr_opt = registry.find_actor<tpaactors::Sequencer<std::string>>(argv[1]);
			if (seqr_opt) {
				return seqr_opt.value();
			}
			switch (seqr_opt.error()) {
				case tpacore::ActorRegistry::FindActorError::NotFound:
					break;
				case tpacore::ActorRegistry::FindActorError::TypeMismatch:
					throw std::runtime_error("Actor with name " + std::string{argv[1]} + " exists but has wrong type");
			}
			// Wait for the sequencer to be created
		}
	}();

	std::cout << "Sequencer found, creating producer..." << std::endl;

	StrProducer producer{"Producer", seqr};
	producer.do_registrations(registry);

	for (int i = 0; i < 100; ++i) {
		producer.submit_string("String " + std::to_string(i));
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	// Cannot tear down cleanly since the sequencer is still running and holds references to
	// the strings we sent
	registry_worker.join();

	return 0;
}
