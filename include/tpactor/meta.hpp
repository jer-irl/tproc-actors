#pragma once

#include <utility>

namespace tpactor {

template <typename T>
class UniqueResourcePtr;

template <typename TupleLikeT, typename T>
struct ContainsType;

template<template <typename...> class TupleLike, typename T, typename... Ts>
struct ContainsType<TupleLike<Ts...>, T> {
    static constexpr bool value = (std::same_as<T, Ts> || ...);
};

template <typename TupleLikeT, typename T>
constexpr static bool ContainsTypeV = ContainsType<TupleLikeT, T>::value;

class Actor;
template <typename T>
concept ActorImpl = std::derived_from<std::remove_cvref_t<T>, Actor> && requires {
    typename std::remove_cvref_t<T>::Receivables;
    typename std::remove_cvref_t<T>::Destructables;
};

template <typename T, typename M>
concept ReceiverOf = ActorImpl<T> && ContainsTypeV<typename std::remove_cvref_t<T>::Receivables, M> && requires(T t, UniqueResourcePtr<std::add_const_t<M>> resource) {
    { t.on_message_impl(std::move(resource)) } -> std::same_as<void>;
};

template <typename... Ts>
struct TypeContainer {};

} // namespace tpactor
