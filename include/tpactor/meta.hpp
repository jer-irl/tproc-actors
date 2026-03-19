#pragma once

#include <utility>

namespace tpactor {

template <typename T>
class UniqueResourcePtr;

template <typename T, typename M>
concept ReceiverOf = requires(T t, UniqueResourcePtr<M> resource) {
    { t.on_message_impl(std::move(resource)) } -> std::same_as<void>;
};

class Actor;
template <typename T>
concept ActorImpl = std::derived_from<T, Actor> && requires {
    typename T::Receivables;
    typename T::Destructables;
};

template <typename... Ts>
struct TypeContainer {};

} // namespace tpactor
