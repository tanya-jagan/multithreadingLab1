#include "bounded_queue.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <cassert>

void test_single_PC()
{
    BoundedQueue<int> q(5);

    std::thread producer([&]
                         {
        for(int i = 0; i < 10; i++) q.push(i);
        q.close(); });

    std::thread consumer([&]
                         {
                             int val;
                             int sum = 0;
                             while (q.pop(val))
                             {
                                 sum += val;
                             }
                             assert(sum == 45); // sum(0-9)
                         });

    producer.join();
    consumer.join();
    std::cout << "Single P/C test passed!\n";
}

int main()
{
    test_single_PC();
    return 0;
}