#include "bounded_queue.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <getopt.h>
#include <cassert>

using namespace std;

bool verbose = false;

// ---------------- Utility: producers & consumers ----------------
void producerFunc(BoundedQueue<int> &q, int id, int num_items, atomic<int> &produced)
{
    for (int i = 0; i < num_items; ++i)
    {
        q.push(i + id * 1000);
        ++produced;
        if (verbose && i % 100 == 0)
            cout << "Producer " << id << " pushed " << i << endl;
    }
}

void consumerFunc(BoundedQueue<int> &q, int id, atomic<int> &consumed)
{
    int val;
    while (q.pop(val))
    {
        ++consumed;
        if (verbose && consumed % 100 == 0)
            cout << "Consumer " << id << " popped " << val << endl;
    }
}

// ---------------- Test 1: Basic FIFO ----------------
void test_basic_fifo(int P, int C)
{
    (void)P; (void)C; // unused
    BoundedQueue<int> q(3);

    q.push(10);
    q.push(20);
    q.push(30);

    int x;
    bool ok;
    ok = q.pop(x); assert(ok && x == 10);
    ok = q.pop(x); assert(ok && x == 20);
    ok = q.pop(x); assert(ok && x == 30);
    assert(q.size() == 0);

    cout << "test_basic_fifo passed\n";
}

// ---------------- Test 2: Backpressure (Blocking Push) ----------------
void test_backpressure(int P, int C)
{
    (void)P; (void)C;
    BoundedQueue<int> q(2);
    atomic<bool> push_done = false;

    thread producer([&] {
        q.push(1);
        q.push(2);
        // This push should block until a pop occurs
        q.push(3);
        push_done = true;
    });

    this_thread::sleep_for(chrono::milliseconds(100));
    assert(!push_done.load()); // should still be blocked

    int val;
    bool ok = q.pop(val);
    assert(ok && val == 1);

    producer.join();
    assert(push_done.load());
    cout << "test_backpressure passed\n";
}

// ---------------- Test 3: Spurious Wakeup Prevention ----------------
void test_spurious_wakeup(int P, int C)
{
    (void)P; (void)C;
    BoundedQueue<int> q(1);
    atomic<bool> pop_done = false;

    thread consumer([&] {
        int val;
        q.pop(val); // should block until producer pushes
        pop_done = true;
    });

    this_thread::sleep_for(chrono::milliseconds(100));
    assert(!pop_done.load()); // still blocked

    thread producer([&] {
        this_thread::sleep_for(chrono::milliseconds(200));
        q.push(42);
    });

    consumer.join();
    producer.join();
    assert(pop_done.load());
    cout << "test_spurious_wakeup passed\n";
}

// ---------------- Test 4: Multi-Producer / Multi-Consumer ----------------
void test_parallel_produce_consume(int P, int C)
{
    BoundedQueue<int> q(20);
    atomic<int> produced = 0, consumed = 0;
    const int items_per_producer = 1000;

    vector<thread> producers, consumers;

    for (int i = 0; i < P; ++i)
        producers.emplace_back(producerFunc, ref(q), i, items_per_producer, ref(produced));

    for (int i = 0; i < C; ++i)
        consumers.emplace_back(consumerFunc, ref(q), i, ref(consumed));

    for (auto &p : producers)
        p.join();

    q.close(); // stop consumers cleanly

    for (auto &c : consumers)
        c.join();

    cout << "Produced: " << produced << ", Consumed: " << consumed << endl;
    assert(produced == consumed);
    cout << "test_parallel_produce_consume passed\n";
}

// ---------------- Test 5: Clean Shutdown ----------------
void test_shutdown(int P, int C)
{
    BoundedQueue<int> q(10);
    atomic<int> produced = 0, consumed = 0;

    vector<thread> producers, consumers;

    for (int i = 0; i < P; ++i)
        producers.emplace_back(producerFunc, ref(q), i, 200, ref(produced));

    for (int i = 0; i < C; ++i)
        consumers.emplace_back(consumerFunc, ref(q), i, ref(consumed));

    this_thread::sleep_for(chrono::milliseconds(200));
    q.close(); // simulate external shutdown

    for (auto &p : producers) p.join();
    for (auto &c : consumers) c.join();

    cout << "Produced: " << produced << ", Consumed: " << consumed << endl;
    assert(consumed <= produced);
    cout << "test_shutdown passed\n";
}

int main(int argc, char *argv[])
{
    int P = 2, C = 2, test_case = 1;
    int opt;
    while ((opt = getopt(argc, argv, "p:c:t:v")) != -1)
    {
        switch (opt)
        {
        case 'p': P = stoi(optarg); break;
        case 'c': C = stoi(optarg); break;
        case 't': test_case = stoi(optarg); break;
        case 'v': verbose = true; break;
        default:
            cerr << "Usage: " << argv[0]
                 << " -p <producers> -c <consumers> -t <test#> [-v]\n";
            cerr << "Tests: 1=FIFO, 2=Backpressure, 3=Wakeup, 4=Parallel, 5=Shutdown\n";
            return 1;
        }
    }

    if (P <= 0 || C <= 0)
    {
        cerr << "Error: number of producers and consumers must be > 0\n";
        return 1;
    }

    if (test_case < 1 || test_case > 5)
    {
        cerr << "Error: invalid test case number (1–5)\n";
        return 1;
    }

    auto start = chrono::high_resolution_clock::now();

    switch (test_case)
    {
    case 1: test_basic_fifo(P, C); break;
    case 2: test_backpressure(P, C); break;
    case 3: test_spurious_wakeup(P, C); break;
    case 4: test_parallel_produce_consume(P, C); break;
    case 5: test_shutdown(P, C); break;
    }

    auto end = chrono::high_resolution_clock::now();
    cout << "Elapsed: "
         << chrono::duration_cast<chrono::milliseconds>(end - start).count()
         << " ms\n";
    return 0;
}
