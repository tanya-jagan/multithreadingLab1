#include "bounded_queue.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <getopt.h>

using namespace std;

BoundedQueue<int> q(10);
atomic<int> produced(0), consumed(0);
bool verbose = false;

// Basic producer
void producer_func(int id, int num_items)
{
    for (int i = 0; i < num_items; ++i)
    {
        q.push(i + id * 1000);
        ++produced;
        if (verbose && i % 100 == 0)
            cout << "Producer " << id << " pushed " << i << endl;
    }
}

// Basic consumer
void consumer_func(int id)
{
    int val;
    while (q.pop(val))
    {
        ++consumed;
        if (verbose && consumed % 100 == 0)
            cout << "Consumer " << id << " popped " << val << endl;
    }
}

// ---------- Test cases ----------
void test_basic(int P, int C)
{
    cout << "Running test_basic with " << P << " producers, " << C << " consumers\n";
    vector<thread> threads;
    for (int i = 0; i < P; ++i)
        threads.emplace_back(producer_func, i, 500);
    for (int i = 0; i < C; ++i)
        threads.emplace_back(consumer_func, i);

    for (int i = 0; i < P; ++i)
        threads[i].join();
    q.close();
    for (int i = P; i < P + C; ++i)
        threads[i].join();

    cout << "Produced: " << produced << ", Consumed: " << consumed << endl;
}

void test_shutdown(int P, int C)
{
    cout << "Running test_shutdown\n";
    vector<thread> threads;
    for (int i = 0; i < P; ++i)
        threads.emplace_back(producer_func, i, 100);
    for (int i = 0; i < C; ++i)
        threads.emplace_back(consumer_func, i);

    this_thread::sleep_for(chrono::milliseconds(200));
    q.close();
    for (auto &t : threads)
        t.join();
    cout << "Closed queue early; consumers exit cleanly.\n";
}

void test_blocking_behavior()
{
    cout << "Running test_blocking_behavior (1 producer, 1 consumer)\n";
    BoundedQueue<int> local_q(2);
    thread prod([&]()
                {
        for (int i = 0; i < 10; ++i) {
            local_q.push(i);
            cout << "Produced " << i << endl;
        }
        local_q.close(); });
    thread cons([&]()
                {
        int val;
        while (local_q.pop(val)) {
            cout << "Consumed " << val << endl;
            this_thread::sleep_for(chrono::milliseconds(100));
        } });
    prod.join();
    cons.join();
}

// main
int main(int argc, char *argv[])
{
    int opt;
    int P = 2, C = 2, test_case = 1;

    while ((opt = getopt(argc, argv, "p:c:t:v")) != -1)
    {
        switch (opt)
        {
        case 'p':
            P = stoi(optarg);
            break;
        case 'c':
            C = stoi(optarg);
            break;
        case 't':
            test_case = stoi(optarg);
            break;
        case 'v':
            verbose = true;
            break;
        default:
            cerr << "Usage: " << argv[0] << " -p <producers> -c <consumers> -t <test#> [-v]\n";
            return 1;
        }
    }

    auto start = chrono::high_resolution_clock::now();

    switch (test_case)
    {
    case 1:
        test_basic(P, C);
        break;
    case 2:
        test_shutdown(P, C);
        break;
    case 3:
        test_blocking_behavior();
        break;
    default:
        cerr << "Invalid test case\n";
        return 1;
    }

    auto end = chrono::high_resolution_clock::now();
    cout << "Elapsed: "
         << chrono::duration_cast<chrono::milliseconds>(end - start).count()
         << " ms\n";
    return 0;
}
