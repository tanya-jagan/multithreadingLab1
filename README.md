# multithreadingLab1

Goal
Build a reusable BoundedQueue<T> with blocking push/pop that is safe under high contention.

Requirements
• Use std::mutex and std::condition variable. 
• Support N producers and M consumers.
• Prevent spurious wakeups by using while-loops on predicates.
• Support clean shutdown (close() or poison pill).
• Enforce backpressure: producers block when full.

Deliverables
• Implementation: bounded queue.hpp/.cpp with unit tests.
• Driver that streams K items through the queue and verifies ordering.
• Performance benchmarks with varying (N, M, capacity).

--------------------------------------------------------------------------------

BoundedQueue is a thread safe, templated FIFO queue that supports multiple producers and consumers. It has a maximum capacity, provides blocking when full or empty, and allows for a clean shutdown when no more items will be added.

Push:
- acquires mutex
- waits until the queue has space or until it's closed
- if the queue is closed it returns false
- otherwise, it inserts the item at the end of the queue
- increments the push counter
- notifies a waiting consumer if they exist 

Pop: 
- acquires mutex
- waits until queue isn't empty or is closed
- if the queue is empty and closed it returns false
- otherwise, it removes the item from the front of the queue
- increments the pop counter
- notifies a waiting producer if they exist

1. Thread-Safe Access: std::mutex is used for accessing std:deque, and all operations that involve the queue acquire the lock to prevent race conditions.

2. Blocking Push/Pop: push() blocks when the queue is full and waits until space is available to add items, whereas pop() blocks if the queue is empty, waiting for the producer to insert an item. push() and pop() use std::condition_variable to block and wake threads.

3. Multi Producer/Consumer: the queue allows for N producers and M consumers to operate concurrently. notFull is used to wake producers when there's space and notEmpty is used to wake consumers when there's new items available in the queue.

4. Spurious Wakeup: wakes are enclosed in a while loop that checks the queue state so that if a thread spuriously wakes up, it must recheck the queue condition and wait when needed.

5. Metrics: Successful pushes and pops are counted uses std::atomic<uint64_t> counters.

6. Move Semantics: There is an overloaded(pushT&& item) for efficiently inserting movable objects rather than unnecessarily copying.

--------------------------------------------------------------------------------
Tests 

"Usage: " << argv[0]
                 << " -p <producers> -c <consumers> -t <test#> -q capacity -k <numItems> [-x]\n";
"Tests: 1=FIFO, 2=Backpressure, 3=Wakeup, 4=Parallel, 5=Shutdown\n";

1. Basic FIFO: verifies that K items are consumed in produced order.
2. Backpressure: verifies that producer blocks when queue is full until a consumer pops.
3. Spurious Wakeup: verifies that blocked consumers/producers are only woken when they should be.
4. Multi Producer/Consumer: verifies that multiple producers and consumers can operate concurrently.
5. Shutdown: verifies that calling close() allows threads to exit gracefully without further pushes.

--------------------------------------------------------------------------------
Performance Analysis 
- There is a performance improvement from overlapping producer/consumer work. In an ideal case it would be near-linear, but due to factors like context-switch overhead and mutex contention, the speedup plateaus with more threads added. You would expect that for 1 producer/1 consumer as the baseline, having 2 producers/2 consumers would be 1.7x faster and 4 producers/4 consumers would be 2.3x faster.

Testing Conditions: run on local machine w/ Intel Core Ultra 7 Series 2, 16 GB RAM. OS used is Windows 11 + WSL2 Ubuntu 22.04. Compiler used is g++. No signficant processes running during tests. 

Observed Performance
- For small queue capacities, contention dominates (threads frequently block).
- For larger capacities, performance improves slightly due to less frequent waiting, but the queue still remains the bottleneck because all threads share one mutex.
- The cost of std::unique_lock and std::condition_variable wakeups grows with the number of threads.

Scaling Limits
- Lock Contention: A single mutex serializes all push/pop access.
- Context Switching: Each blocked thread incurs OS scheduling overhead.

Possible Improvements
- Lock-free implementation: Use atomic operations and a circular buffer for a non-blocking queue.

AI used for boilerplate code, making more thorough tests, introducing better atomicity