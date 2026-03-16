#pragma once

#include "mem.hpp"
#include "rings.hpp"

namespace tpactor {

class Actor {
public:
    virtual ~Actor() = default;
    Actor();
    auto send(UniqueResourcePtr<void> message) -> void;

private:
    virtual auto on_message(UniqueResourcePtr<void>) -> void = 0;

    struct alignas(64) Message {
        UniqueResourcePtr<void> payload;
    };
    MpscRing<Message, 512> recv_queue_;
};

} // namespace tpactor
