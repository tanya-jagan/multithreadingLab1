#pragma once // combines using #ifndef, #define, and #endif for .hpp!!

#include <mutex>
#include <condition_variable> // synch prim used w/ mutex to block threads until 
// another thread modifies condition (shared variable) and notifies condition_variable

#include <stdexcept>
#include <queue>

template <typename T>
class BoundedQueue
{
public:
    // constructor: specify max capacity (>0)
    BoundedQueue(size_t capacity) : capacity(capacity), closed(false)
    {
        if (capacity == 0)
        {
            throw std::invalid_argument("capacity must be > 0");
        }
    }

    /**
     * blocks until there's space in the queue, throws if queue is clsoed
     * requirements: use std::mutex and std::condition_variable, support
     * N producers and M consumers */
    bool push(const T& item)
    {
        std::unique_lock<std::mutex> lock(mutex);

        // wait until queue isn't full or closed
        notFull.wait(lock, [this]
                       { return queue.size() < capacity || closed; });

        if (closed)
        {
            throw std::runtime_error("queue is closed");
        }

        queue.push(item);

        // signal that queue has data for consumers
        notEmpty.notify_one();
    }

    /**
     * blocks until available item, return false if
     * queue is closed and empty */
    bool pop(T& item)
    {
        std::unique_lock<std::mutex> lock(mutex);

        // wait until queue has data or is closed
        notEmpty.wait(lock, [this]
                       { return !queue.empty() || closed; });

        // stop if empty and closed 
        if (queue.empty())
        {
            return false;
        }

        item = std::move(queue.front());
        queue.pop();

        // signal that  space is available for producers
        notFull.notify_one();
    }

    /**
     * clean shutdown, no more pushes allowed
     * wakes up all waiting P&C to tell them to get out */
    void close()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            closed = true;
        }

        // wake up the waiting P&C to check the "closed_" flag
        notFull.notify_all();
        notEmpty.notify_all();
    }

    // returns # of curr items 
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.size();
    }

    private:
        size_t capacity;
        bool closed;                           // shutdown flag
        mutable std::mutex mutex;                // for sharing 
        std::condition_variable notFull;      // space avail signal
        std::condition_variable notEmpty;     // data avail signal
        std::queue<T> queue;                   // data structure itself 
};