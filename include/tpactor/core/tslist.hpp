#pragma once

#include <atomic>
#include <iterator>
#include <optional>
#include <vector>
#include <memory>

namespace tpactor::core {

// Single-producer, multiple observer list.  New nodes are permanent.
template <typename T>
class SpMoList {
	struct Node {
		std::atomic<Node*> prev{nullptr};
		std::atomic<Node*> next{nullptr};
		std::optional<T> value;
	};

	Node sent_{&sent_, &sent_, std::nullopt};

	// for raii, TODO improve
	std::vector<std::unique_ptr<Node>> nodes_;

public:
	SpMoList() = default;

	auto size() const -> std::size_t {
		return nodes_.size();
	}

	auto push(T value) {
		auto node = std::make_unique<Node>();
		node->value = std::move(value);
		node->next.store(&sent_, std::memory_order_relaxed);

		Node* old_tail = sent_.prev.load(std::memory_order_relaxed);
		node->prev.store(old_tail, std::memory_order_relaxed);

		old_tail->next.store(node.get(), std::memory_order_release);
		sent_.prev.store(node.get(), std::memory_order_release);

		nodes_.push_back(std::move(node));
	}

	class Iterator {
		Node* current_;
	public:
		Iterator(Node* start) : current_(start) {}
		Iterator& operator++() {
			current_ = current_->next.load(std::memory_order_acquire);
			return *this;
		}
		Iterator& operator++(int) {
			Iterator copy = *this;
			++(*this);
			return copy;
		}
		Iterator& operator--() {
			current_ = current_->prev.load(std::memory_order_acquire);
			return *this;
		}
		Iterator& operator--(int) {
			Iterator copy = *this;
			--(*this);
			return copy;
		}

		bool operator==(const Iterator& other) const {
			return current_ == other.current_;
		}
		bool operator!=(const Iterator& other) const {
			return !(*this == other);
		}
		T& operator*() const {
			return current_->value.value();
		}
	};

	Iterator begin() {
		return Iterator{sent_.next.load(std::memory_order_acquire)};
	}
	Iterator end() {
		return Iterator{&sent_};
	}
	std::reverse_iterator<Iterator> rbegin() {
		return std::reverse_iterator{end()};
	}
	std::reverse_iterator<Iterator> rend() {
		return std::reverse_iterator{begin()};
	}
};

} // namespace tpactor::core
