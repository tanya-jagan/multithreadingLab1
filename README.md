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

Stretch Goals (for extra credit)
• Add timed operations (try push for, try pop for).
• Collect queue depth metrics and visualize them.