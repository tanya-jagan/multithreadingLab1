#include "bounded_queue.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <getopt.h>
#include <cassert>

using namespace std;

// can be set with -x flag to track progress
bool progressTracker = false;

// producer thread function -- produces numItems items ------------------------------------------------------------------------------------------------------
void producerFunc(BoundedQueue<int> &q, int id, int numItems, atomic<int> &produced)
{
    for (int i = 0; i < numItems; ++i)
    {
        q.push(i + id * 100); // unique values per producer
        ++produced;
        if (progressTracker && i % 100 == 0)
        {
            cout << "Producer " << id << " pushed " << i << endl;
        }
    }
}

// consumer thread function -- consumes until queue is closed and empty  ------------------------------------------------------------------------------------
void consumerFunc(BoundedQueue<int> &q, int id, atomic<int> &consumed)
{
    int val;
    while (q.pop(val))
    {
        ++consumed;
        if (progressTracker && consumed % 100 == 0)
        {
            cout << "Consumer " << id << " popped " << val << endl;
        }
    }
}

void testBasicFIFO(int K)
{
    cout << "starting testBasicFIFO\n";

    BoundedQueue<int> q(100);

    vector<int> produced(K);
    vector<int> consumed;

    for (int i = 0; i < K; ++i)
    {
        produced[i] = i + 1;
    }

    thread p([&]()
             {
                 for (int val : produced)
                 {
                     bool ok = q.push(val);
                     assert(ok); // should always succeed
                 }
                 q.close(); // signal no more items
             });

    thread c([&]()
             {
        int val;
        while (q.pop(val))
        {
            consumed.push_back(val);
        } });

    p.join();
    c.join();

    if (consumed != produced)
    {
        cerr << "Error: consumed items do not match produced items\n";
        cerr << "Produced: ";
        for (int v : produced)
        {
            cerr << v << " ";
        }
        cerr << "\nConsumed: ";
        for (int v : consumed)
        {
            cerr << v << " ";
        }
        cerr << endl;
        assert(false);
    }

    cout << "testBasicFIFO passed: ordering verified for " << K << " items!\n";
}

void testBackPressure()
{
    cout << "starting testBackpressure\n";

    const int queueCapacity = 3;
    BoundedQueue<int> q(queueCapacity);
    atomic<bool> needToBlock = false;
    atomic<bool> pushDone = false;

    // producer thread - will block on 4th push
    thread producer([&]()
                    {
        cout << "Producer pushing 1\n";
        bool ok = q.push(1);
        assert(ok);

        cout << "Producer pushing 2\n";
        ok = q.push(2);
        assert(ok);

        cout << "Producer pushing 3\n";
        ok = q.push(3);
        assert(ok);

        needToBlock = true;
        cout << "Producer pushing 4 (should block)...\n";
        ok = q.push(4); // should block here until a consumer pops
        assert(ok);
        cout << "Producer pushed 4\n";
        pushDone = true; });

    while (!needToBlock)
        ;
    cout << "Main thread sleeping 2 seconds to ensure producer is blocked on push(4)\n";
    this_thread::sleep_for(chrono::seconds(2));
    assert(!pushDone.load() && "Producer should be blocked on push(4)");

    // consume an item to free up room in the queue
    int val;
    bool ok = q.pop(val);
    assert(ok && val == 1 && "Popped value should be 1");

    // producer should now be able to push 4
    producer.join();
    assert(pushDone.load() && "Producer should have completed pushing 4 items");
    cout << "testBackPressure passed\n";
}

void testSpuriousWakeup(int P, int C)
{
    (void)P;
    (void)C;
    BoundedQueue<int> q(1);
    atomic<bool> pop_done = false;

    thread consumer([&]
                    {
        int val;
        q.pop(val); // should block until producer pushes
        pop_done = true; });

    this_thread::sleep_for(chrono::milliseconds(100));
    assert(!pop_done.load()); // still blocked

    thread producer([&]
                    {
        this_thread::sleep_for(chrono::milliseconds(200));
        q.push(42); });

    consumer.join();
    producer.join();
    assert(pop_done.load());
    cout << "testSpuriousWakeup passed\n";
}

void testMultiProducerConsumer(int P, int C, int capacity)
{
    cout << "starting testMultiProducerConsumer with " << P << " producers and " << C << " consumers\n";
    assert(P > 0 && C > 0 && "P and C must be > 0");

    // const int queueCapacity = 10;
    const int itemsPerProducer = 1000;
    BoundedQueue<int> q(capacity);
    atomic<int> produced = 0;
    atomic<int> consumed = 0;

    vector<thread> producers;
    vector<thread> consumers;

    auto start = chrono::high_resolution_clock::now(); // timer start

    // producers start!
    for (int i = 0; i < P; ++i)
    {
        producers.emplace_back(producerFunc, ref(q), i, itemsPerProducer, ref(produced));
    }

    // consumers start!
    for (int i = 0; i < C; ++i)
    {
        consumers.emplace_back(consumerFunc, ref(q), i, ref(consumed));
    }

    // wait for producers to finish
    for (auto &p : producers)
    {
        p.join();
    }

    // close the queue to stop consumers, clean finish
    q.close();

    for (auto &c : consumers)
    {
        c.join();
    }

    auto end = chrono::high_resolution_clock::now(); // timer stop
    chrono::duration<double> elapsed = end - start;

    cout << "Produced: " << produced << "\nConsumed: " << consumed
         << "\nElapsed Time: " << elapsed.count() << " s\n";
    assert(produced == P * itemsPerProducer && "Produced count mismatch");
    assert(produced == consumed && "Produced and consumed counts should match");
    cout << "testMultiProducerConsumer passed successfully!!!!\n";
}

void testShutdown(int P, int C)
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

    for (auto &p : producers)
    {
        p.join();
    }
    for (auto &c : consumers)
    {
        c.join();
    }

    cout << "Produced: " << produced << ", Consumed: " << consumed << endl;
    assert(consumed <= produced);
    cout << "testShutdown passed\n";
}

int main(int argc, char *argv[])
{
    int P = 2, C = 2, testCase = 1, capacity = 10, K = 1000;
    int opt;
    while ((opt = getopt(argc, argv, "p:c:t:q:k:x")) != -1)
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
            testCase = stoi(optarg);
            break;
        case 'q':
            capacity = stoi(optarg);
            break;
        case 'k':
            K = stoi(optarg);
            break;
        case 'x':
            progressTracker = true;
            break;
        default:
            cerr << "Usage: " << argv[0]
                 << " -p <producers> -c <consumers> -t <test#> -q capacity -k <numItems> [-x]\n";
            cerr << "Tests: 1=FIFO, 2=Backpressure, 3=Wakeup, 4=Parallel, 5=Shutdown\n";
            return 1;
        }
    }

    if (P <= 0 || C <= 0 || capacity <= 0 || testCase < 1 || testCase > 5 || K <= 0)
    {
        cerr << "Invalid arguments.\n";
        return 1;
    }

    auto start = chrono::high_resolution_clock::now();

    switch (testCase)
    {
    case 1:
        testBasicFIFO(K);
        break;
    case 2:
        testBackPressure();
        break;
    case 3:
        testSpuriousWakeup(P, C);
        break;
    case 4:
        testMultiProducerConsumer(P, C, capacity);
        break;
    case 5:
        testShutdown(P, C);
        break;
    }

    auto end = chrono::high_resolution_clock::now();
    cout << "Elapsed: "
         << chrono::duration_cast<chrono::milliseconds>(end - start).count()
         << " ms\n";
    return 0;
}
