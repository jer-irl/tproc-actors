#pragma once

#include "mem.hpp"
#include "rings.hpp"

#include <concepts>
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

    auto on_release_impl(UniqueResourcePtr<T> resource) -> void {
        delete resource.get();
    }
    auto register_destructor(ActorRegistry& registry) -> void {
        registry.register_destructor(+[](void* resource) {
            delete reinterpret_cast<T*>(resource);
        });
    }
};

template <typename T>
struct Receiver {
    template <typename ReceivableT, typename Self>
    auto on_message_helper(this Self&& self, UniqueResourcePtr<void>& message) -> bool {
        if (typeid(ReceivableT) == message.type_info()) {
            self.on_message_impl(std::move(reinterpret_cast<UniqueResourcePtr<ReceivableT>&&>(message)));
            return true;
        }
        return false;
    }
};

template <typename... ReceivableTs>
struct Receivables;


template <typename... DestructableTs>
struct Destructables;

template <typename ReceivablesT, typename DestructablesT>
class Actor;

template <typename... ReceivableTs, typename... DestructableTs>
class Actor<Receivables<ReceivableTs...>, Destructables<DestructableTs...>> : private Receiver<ReceivableTs>..., private Destructor<DestructableTs>... {
private:
    struct alignas(64) Message {
        UniqueResourcePtr<void> payload{};
    };

public:
    virtual ~Actor() = default;
    explicit Actor(std::string_view name) : Actor{name, *ActorRegistry::instance()} {}
    Actor(std::string_view name, ActorRegistry& registry) : tproc_id_(registry.tproc_id())
    {
        auto res = registry.register_actor(name, *this);
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
        while (self.recv_queue_.push(Message{std::move(erased_resource)})) {
            // Wait for space in the queue
        }
    }

    template <typename Self, typename T>
    auto release_to(this Self&& self, UniqueResourcePtr<T> resource)
        requires (self.on_release_impl(std::declval<UniqueResourcePtr<T>>()))
    {
        while (!self.recv_queue_.push(Message{.kind = Message::Kind::Release, .payload = std::move(reinterpret_cast<UniqueResourcePtr<void>&&>(resource))})) {
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

protected:
    template <typename TargetActorT, typename T>
    auto do_send(std::string_view target_actor_name, UniqueResourcePtr<T> message) -> void {
        auto target_actor_res = ActorRegistry::instance()->get().find_actor<TargetActorT>(target_actor_name);
        if (!target_actor_res) {
            throw std::runtime_error("Failed to find target actor: " + target_actor_res.error());
        }
        auto target_actor_opt = target_actor_res.value();
        if (!target_actor_opt) {
            throw std::runtime_error("Target actor not found: " + std::string{target_actor_name});
        }
        auto& target_actor = target_actor_opt.value().get();
        target_actor.send_to(std::move(message));
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

    template <typename Self, typename T>
    auto on_release(this Self&& self, UniqueResourcePtr<T> resource) -> void {
        if (!resource.get()) {
            return;
        }
        if (resource.tproc_id() != self.tproc_id_) {
            throw std::runtime_error("Received release request for resource not owned by this threadproc");
        }
        self.on_release_impl(std::move(resource));
    }
    
    MpscRing<Message, 512> recv_queue_;
    std::uint64_t tproc_id_{0};
};

} // namespace tpactor
