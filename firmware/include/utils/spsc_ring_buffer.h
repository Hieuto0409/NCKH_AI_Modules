#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace ppgfw {

template <typename T, size_t Capacity>
class SpscRingBuffer {
    static_assert(Capacity >= 2, "Capacity must be at least two");
    static_assert((Capacity & (Capacity - 1U)) == 0, "Capacity must be a power of two");

public:
    bool push(const T& value) {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next = (head + 1U) & (Capacity - 1U);
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        storage_[head] = value;
        head_.store(next, std::memory_order_release);
        const size_t current = size();
        size_t observed = high_water_mark_.load(std::memory_order_relaxed);
        while (current > observed &&
               !high_water_mark_.compare_exchange_weak(observed, current, std::memory_order_relaxed)) {
        }
        return true;
    }

    bool pop(T& value) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        value = storage_[tail];
        tail_.store((tail + 1U) & (Capacity - 1U), std::memory_order_release);
        return true;
    }

    size_t size() const {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return (head - tail) & (Capacity - 1U);
    }

    size_t capacity() const {
        return Capacity - 1U;
    }

    size_t highWaterMark() const {
        return high_water_mark_.load(std::memory_order_relaxed);
    }

    void clear() {
        tail_.store(head_.load(std::memory_order_acquire), std::memory_order_release);
        high_water_mark_.store(0, std::memory_order_relaxed);
    }

private:
    std::array<T, Capacity> storage_{};
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
    std::atomic<size_t> high_water_mark_{0};
};

}

