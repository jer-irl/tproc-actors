#pragma once

#include <array>
#include <atomic>

namespace tpactor::core {

template <typename T, std::size_t Size>
class MpscRing {
public:
	static_assert(alignof(T) >= 64);

	MpscRing() = default;

	bool push(T&& item) {
		uint64_t tail = append_tail_.load(std::memory_order_acquire);
		uint64_t head = head_.load(std::memory_order_acquire);
		if (tail - head >= Size) {
			return false; // Ring is full
		}

		while (!append_tail_.compare_exchange_weak(tail, tail + 1, std::memory_order_acq_rel)) {
			if (tail - head >= Size) {
				return false; // Ring is full
			}
		}

		buffer_[tail % Size] = std::move(item);

		while (committed_tail_.load(std::memory_order_acquire) != tail) {
			// Wait for our turn to commit
		}
		committed_tail_.store(tail + 1, std::memory_order_release);

		return true;
	}

	bool pop(T& item) {
		uint64_t tail = committed_tail_.load(std::memory_order_acquire);
		uint64_t head = head_.load(std::memory_order_relaxed);
		if (tail == head) {
			return false; // Ring is empty
		}
		item = std::move(buffer_[head % Size]);
		head_.store(head + 1, std::memory_order_release);
		return true;
	}

private:
	std::atomic_uint64_t head_{};
	std::atomic_uint64_t append_tail_{};
	std::atomic_uint64_t committed_tail_{};
	std::array<T, Size> buffer_;
};

} // namespace tpactor::core
