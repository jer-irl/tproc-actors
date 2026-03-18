#pragma once

#include "registry.hpp"

#include <cstring>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace tpactor {

template <typename T>
class UniqueResourcePtr {
public:
	using type = T;

	UniqueResourcePtr() = default;

	UniqueResourcePtr(T* resource, std::uint64_t tproc_id)
		: tproc_id_(tproc_id), resource_(resource) {
	}

	~UniqueResourcePtr() {
		if (resource_) {
			auto& reg = ActorRegistry::instance()->get();
			UniqueResourcePtr<void> to_free = std::move(reinterpret_cast<UniqueResourcePtr<void>&&>(*this));
			reg.release_resource(std::move(to_free));
		}
	}

	UniqueResourcePtr(const UniqueResourcePtr&) = delete;
	UniqueResourcePtr& operator=(const UniqueResourcePtr&) = delete;

	UniqueResourcePtr(UniqueResourcePtr&& other) noexcept {
		*this = std::move(other);
	}
	UniqueResourcePtr& operator=(UniqueResourcePtr&& other) noexcept {
		if (this != &other) {
			std::swap(tproc_id_, other.tproc_id_);
			std::swap(type_info_, other.type_info_);
			std::swap(resource_, other.resource_);
		}
		return *this;
	}

	auto tproc_id() const -> std::uint64_t { return tproc_id_; }
	auto type_info() const -> const std::type_info& { return *type_info_; }

	T* get() const { return resource_; }
	decltype(auto) operator*() const requires (!std::is_void_v<T>) { return *resource_; }
	T* operator->() const { return resource_; }

private:
	friend class ActorRegistry;
	uint64_t tproc_id_{0};
	const std::type_info* type_info_{&typeid(T)};
	T* resource_{nullptr};
};

template <typename T>
UniqueResourcePtr(T*, std::uint64_t) -> UniqueResourcePtr<T>;

} // namespace tpactor
