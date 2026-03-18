#pragma once

#include "mem.hpp"
#include "rings.hpp"

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace tpactor {

class ActorRegistry;

template <typename T>
struct Destructor {
    Destructor() {
        ActorRegistry::instance()->get().register_destructor<T>(+[](void* resource) {
            delete reinterpret_cast<T*>(resource);
        });
    }
};

template <typename T>
struct Receiver {
};

template <typename... ReceivableTs>
struct Receivables;


template <typename... DestructableTs>
struct Destructables;

template <typename DerivedT, typename ReceivablesT, typename DestructablesT>
class Actor;

template <typename DerivedT, typename... ReceivableTs, typename... DestructableTs>
class Actor<DerivedT, Receivables<ReceivableTs...>, Destructables<DestructableTs...>> : private Receiver<ReceivableTs>..., private Destructor<DestructableTs>... {
private:
    struct alignas(64) Message {
        UniqueResourcePtr<void> payload{};
    };

public:
    virtual ~Actor() = default;
    explicit Actor(std::string_view name) : Actor{name, *ActorRegistry::instance()} {}
    Actor(std::string_view name, ActorRegistry& registry) : tproc_id_(registry.tproc_id())
    {
        std::cout << "Registering actor with name " << name << std::endl;
        auto res = registry.register_actor<DerivedT>(name, static_cast<DerivedT&>(*this));
        if (!res) {
            throw std::runtime_error("Failed to register actor: " + res.error());
        }
    }

    template <typename Self>
    static constexpr auto agent_type_name() -> std::string_view {
        return Self::type_name_impl();
    }

    template <typename Self, typename T>
    auto send_to(this Self&& self, UniqueResourcePtr<T> resource)
        requires (std::is_same_v<T, ReceivableTs> || ...) && ( requires { self.on_message_impl(std::declval<UniqueResourcePtr<T>>()); } )
    {
        UniqueResourcePtr<void> erased_resource = std::move(reinterpret_cast<UniqueResourcePtr<void>&&>(resource));
        while (!self.recv_queue_.push(Message{std::move(erased_resource)})) {
            // Wait for space in the queue
        }
    }

    auto tproc_id() const -> std::uint64_t { return tproc_id_; }

    template <typename Self>
    auto poll(this Self&& self) -> bool {
        Message message;
        if (self.recv_queue_.pop(message)) {
            self.on_message(std::move(message.payload));
            return true;
        }
        return false;
    }

private:

    template <typename ReceivableT, typename Self>
    auto on_message_helper(this Self&& self, UniqueResourcePtr<void>& message) -> bool {
        if (typeid(ReceivableT) == message.type_info()) {
            self.on_message_impl(std::move(reinterpret_cast<UniqueResourcePtr<ReceivableT>&&>(message)));
            return true;
        }
        return false;
    }

    template <typename Self>
    auto on_message(this Self&& self, UniqueResourcePtr<void> message) -> void {
        (self.template on_message_helper<ReceivableTs>(message) || ...);
    }

    MpscRing<Message, 512> recv_queue_;
    std::uint64_t tproc_id_{0};
};

} // namespace tpactor
