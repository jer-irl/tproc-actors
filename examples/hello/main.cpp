#include <tpactor/actor.hpp>

class MyActor : public tpactor::Actor {
private:
    auto on_message(tpactor::UniqueResourcePtr<void> message) -> void override {
        // Handle incoming message
        (void) message;
    }
};

int main(void) {
    MyActor actor;
    return 0;
}
