#include "bounded_queue.hpp"
#include <thread>
#include <iostream>
#include <vector>
#include <chrono>

int main()
{
    const int N = 5;     // producers
    const int M = 5;     // consumers
    const int K = 10000; // items
    const int capacity = 100;

    BoundedQueue<int> q(capacity);

    auto start = std::chrono::steady_clock::now();

    // start producer threads
    std::vector<std::thread> producers;
    for (int i = 0; i < N; i++)
    {
        producers.emplace_back([&]
                               {
            for(int j = 0; j < (K/N); j++) {
                q.push(j); // block if full queue
            } });
    }

    // start consumer threads
    std::vector<std::thread> consumers;
    for (int i = 0; i < N; i++)
    {
        consumers.emplace_back([&]
                               {
            for(int j = 0; j < (K/N); j++) {
                q.pop(j); // block if full queue
            } });
    }

    // wait for producers to complete
    for (auto &p : producers)
    {
        p.join();
    }

    q.close(); // when consumers need to stop

    // wait for consumers
    for (auto &c : consumers)
    {
        c.join();
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsedTime = end - start;
    std::cout << "Went through" << K << "items in " << elapsedTime.count() << " seconds.\n";

    return 0;
}