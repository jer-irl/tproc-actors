#pragma once

#include <concepts>
#include <string>
#include <type_traits>
#include <utility>

namespace tpactor {

template <typename T>
class UniqueResourcePtr {
public:
	using type = T;

	UniqueResourcePtr(T* resource, std::string creator)
		: creator_(std::move(creator)), resource_(resource) {}

	~UniqueResourcePtr();

	UniqueResourcePtr(const UniqueResourcePtr&) = delete;
	UniqueResourcePtr& operator=(const UniqueResourcePtr&) = delete;

	UniqueResourcePtr(UniqueResourcePtr&& other) noexcept {
		*this = std::move(other);
	}
	UniqueResourcePtr& operator=(UniqueResourcePtr&& other) noexcept {
		if (this != &other) {
			std::swap(creator_, other.creator_);
			std::swap(resource_, other.resource_);
		}
		return *this;
	}

	T* get() const { return resource_; }
	decltype(auto) operator*() const requires (!std::is_void_v<T>) { return *resource_; }
	T* operator->() const { return resource_; }

private:
	std::string creator_;
	T* resource_{nullptr};
};

} // namespace tpactor
