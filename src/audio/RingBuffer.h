#pragma once
#include <atomic>
#include <cstddef>
#include <cstring>
#include <vector>

// Single-Producer Single-Consumer lock-free ring buffer for audio samples.
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : m_capacity(capacity), m_buffer(capacity), m_read(0), m_write(0) {}

    size_t capacity() const { return m_capacity; }

    size_t availableRead() const {
        size_t w = m_write.load(std::memory_order_acquire);
        size_t r = m_read.load(std::memory_order_relaxed);
        return w - r;
    }

    size_t availableWrite() const {
        return m_capacity - availableRead();
    }

    // Write up to count samples. Returns number actually written.
    size_t write(const float* data, size_t count) {
        size_t avail = availableWrite();
        if (count > avail) count = avail;
        if (count == 0) return 0;

        size_t pos = m_write.load(std::memory_order_relaxed) % m_capacity;
        size_t first = std::min(count, m_capacity - pos);
        std::memcpy(m_buffer.data() + pos, data, first * sizeof(float));
        if (count > first)
            std::memcpy(m_buffer.data(), data + first, (count - first) * sizeof(float));

        m_write.fetch_add(count, std::memory_order_release);
        return count;
    }

    // Read up to count samples. Returns number actually read.
    size_t read(float* data, size_t count) {
        size_t avail = availableRead();
        if (count > avail) count = avail;
        if (count == 0) return 0;

        size_t pos = m_read.load(std::memory_order_relaxed) % m_capacity;
        size_t first = std::min(count, m_capacity - pos);
        std::memcpy(data, m_buffer.data() + pos, first * sizeof(float));
        if (count > first)
            std::memcpy(data, m_buffer.data(), (count - first) * sizeof(float));

        m_read.fetch_add(count, std::memory_order_release);
        return count;
    }

    void reset() {
        m_read.store(0, std::memory_order_relaxed);
        m_write.store(0, std::memory_order_relaxed);
    }

private:
    size_t m_capacity;
    std::vector<float> m_buffer;
    std::atomic<size_t> m_read;
    std::atomic<size_t> m_write;
};
