#include <iostream>
#include <vector>
#include <mutex>
#include <stdexcept>
#include <condition_variable>
class RingBuffer {
public:
    explicit RingBuffer(size_t size) : buffer(size), capacity(size), head(0), tail(0), is_full(false) {}

    // blocking push: Will wait if there's not enough space
    bool push( uint8_t*data, size_t size) {
        std::unique_lock<std::mutex> lock(mutex);
        size_t data_size = size;

        if (data_size > available_space()) {
             cond_var.wait(lock, [=] (){ return (size > occupied_space()); }); // Not enough space to push data
        }

        for (size_t i = 0; i < data_size; ++i) {
            buffer[tail] = data[i];
            tail = (tail + 1) % capacity;
            if (head == tail) {
                is_full = true; // Buffer is now full
            }
        }

        return true; // Data successfully pushed
    }

    // Non-blocking pop: Will return all available data if requested size is too large
    std::vector<uint8_t> pop(size_t size) {
        std::lock_guard<std::mutex> lock(mutex);

        size_t data_size = occupied_space();
        if (data_size == 0) {
            return {}; // Buffer is empty, return an empty vector
        }

        // If the requested size is larger than available data, adjust the size
        size_t pop_size = std::min(size, data_size);
        std::vector<uint8_t> data(pop_size);

        for (size_t i = 0; i < pop_size; ++i) {
            data[i] = buffer[head];
            head = (head + 1) % capacity;
            is_full = false; // Buffer is no longer full
        }
         cond_var.notify_one(); // Notify one waiting thread
        return data; // Return the popped data
    }

    size_t occupied_space() const {
        if (is_full) {
            return capacity; // If full, return the capacity
        }
        if (tail >= head) {
            return tail - head;
        }
        return capacity - head + tail;
    }

    size_t available_space() const {
        return capacity - occupied_space(); // Available space in the buffer
    }

private:
    std::vector<uint8_t> buffer;
    size_t capacity;
    size_t head;
    size_t tail;
    mutable std::mutex mutex;
    bool is_full;
    std::condition_variable cond_var;
};


