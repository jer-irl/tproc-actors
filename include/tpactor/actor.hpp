#pragma once

#include <tpactor/mem.hpp>
#include <tpactor/meta.hpp>
#include <tpactor/rings.hpp>

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace tpactor {

class ActorRegistry;

class Actor {
private:
    struct alignas(64) Message {
        UniqueResourcePtr<void> payload{};
    };

public:
    virtual ~Actor() = default;
    explicit Actor(std::string_view name) : Actor{name, ActorRegistry::instance()->get().tproc_id()} {}
    Actor(std::string_view name, std::uint64_t tproc_id) : tproc_id_(tproc_id), name_(name)
    {
        std::cout << "Registering actor with name " << name << std::endl;
    }

    template <typename Self>
    auto do_registrations(this Self&& self, ActorRegistry& registry)
        requires ActorImpl<std::remove_cvref_t<Self>>
    {
        auto res = registry.register_actor<Self>(self.name_, self);
        if (!res) {
            throw std::runtime_error("Failed to register actor: " + res.error());
        }
        self.register_destructors();
    }

    template <typename T, ReceiverOf<T> Self>
    auto send_to(this Self&& self, UniqueResourcePtr<T> resource) -> void {
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
    template <typename Self>
    auto on_message(this Self&& self, UniqueResourcePtr<void> message) -> void {
        self.on_message_helper(message, TypeContainer<typename std::remove_cvref_t<Self>::Receivables>{});
    }
    template <typename Self, template <typename...> class TupleLike, typename... ReceivableTs>
    auto on_message_helper(this Self&& self,UniqueResourcePtr<void>& message, TypeContainer<TupleLike<ReceivableTs...>>) -> void
        requires (ReceiverOf<Self, ReceivableTs> && ...)
    {
        (self.template try_handle_message<ReceivableTs>(message) || ...);
    }

    template <typename ReceivableT, typename Self>
        requires ReceiverOf<Self, ReceivableT>
    auto try_handle_message(this Self&& self, UniqueResourcePtr<void>& message) -> bool {
        if (typeid(ReceivableT) == message.type_info()) {
            self.on_message_impl(std::move(reinterpret_cast<UniqueResourcePtr<ReceivableT>&&>(message)));
            return true;
        }
        return false;
    }

    template <typename Self>
    auto register_destructors(this Self&& self) -> void {
        self.register_destructors_helper(TypeContainer<typename std::remove_reference_t<Self>::Destructables>{});
    }

    template <template <typename...> class TupleLike, typename... DestructableT>
    auto register_destructors_helper(TypeContainer<TupleLike<DestructableT...>>) -> void {
        (ActorRegistry::instance()->get().template register_destructor<DestructableT>(+[](void* resource) {
            delete reinterpret_cast<DestructableT*>(resource);
        }), ...);
    }

    MpscRing<Message, 512> recv_queue_;
    std::uint64_t tproc_id_{0};
    std::string name_;
};

} // namespace tpactor
