#pragma once // combines using #ifndef, #define, and #endif for .hpp!!

#include <mutex>
#include <condition_variable> // synch prim used w/ mutex to block threads until
// another thread modifies condition (shared variable) and notifies condition_variable
#include <deque>
#include <stdexcept>
#include <atomic>

/** templated class can hold any data type compiler won't generate code for template
 * until it's instantiated, and implementation in .cpp can't be seen by compiler when
 * generating code so we define the class in the header */

template <typename T>
class BoundedQueue
{
public:
    // constructor: specify max capacity (>0)
    BoundedQueue(size_t capacity) : capacity_(capacity), closed_(false), pushes_(0), pops_(0)
    {
        if (capacity_ == 0)
        {
            throw std::invalid_argument("capacity must be > 0");
        }
    }

    // blocks until there's space in the queue, throws if queue is closed
    bool push(const T &item)
    {
        // acquire lock on mutex
        std::unique_lock<std::mutex> lock(mutex);

        // wait until queue isn't full or closed - avoids spurious wakeup
        while(!(queue.size() < capacity_ || closed_))
        {
            notFull.wait(lock);
        }

        // no more items allowed if queue closed
        if (closed_)
        {
            return false;
        }

        // insert item to end of deque
        queue.push_back(item);
        // update push metric
        ++pushes_;

        // signal that queue has data for some waiting consumer
        notEmpty.notify_one();

        return true;
    }

    // blocks until there's space in the queue, throws if queue is closed
    // accepts rvalue reference for move efficiency
    bool push(T &&item)
    {
        std::unique_lock<std::mutex> lock(mutex);

        // wait until queue isn't full or closed - avoids spurious wakeup
        while(!(queue.size() < capacity_ || closed_))
        {
            notFull.wait(lock);
        }

        // queue closed
        if (closed_)
        {
            return false;
        }

        // insert item to end of deque
        queue.push_back(std::move(item));
        // update push metric
        ++pushes_;

        // signal that queue has data for some waiting consumer
        notEmpty.notify_one();

        return true;
    }

    // blocks until available item, return false if queue is closed and empty
    bool pop(T &item)
    {
        // acquire lock on mutex
        std::unique_lock<std::mutex> lock(mutex);

        // wait until queue has data or is closed
        while(queue.empty() && !closed_)
        {
            notEmpty.wait(lock);
        }

        // stop if empty and closed aka end of stream for consumer
        if (queue.empty() && closed_)
        {
            return false;
        }

        // remove item from front of deque
        item = std::move(queue.front());
        queue.pop_front();
        // update pop metric
        ++pops_;

        // signal that  space is available for producers
        notFull.notify_one();
        return true;
    }

    // clean shutdown, no more pushes allowed
    // wakes up all waiting P & C to tell them to get out
    void close()
    {
        {
            // stop more pushes
            std::lock_guard<std::mutex> lock(mutex);
            closed_ = true;
        }

        // wake up the waiting P & C to check the "closed_" flag
        // now they can exit if they want
        notFull.notify_all();
        notEmpty.notify_all();
    }

    // returns # of curr items
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.size();
    }

    bool closed() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return closed_;
    }

    size_t capacity() const { return capacity_; }
    uint64_t pushes() const { return pushes_.load(); }
    uint64_t pops() const { return pops_.load(); }

private:
    const size_t capacity_;
    bool closed_;                     // shutdown flag
    mutable std::mutex mutex;         // for sharing
    std::condition_variable notFull;  // space avail signal
    std::condition_variable notEmpty; // data avail signal
    std::deque<T> queue;              // data structure itself

    std::atomic<uint64_t> pushes_;
    std::atomic<uint64_t> pops_;
};