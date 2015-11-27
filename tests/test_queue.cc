//
//  test_queue.cc
//

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <chrono>
#include <vector>
#include <queue>
#include <set>

#include "log.h"
#include "queue_atomic.h"

#include <cppunit/TestCase.h>
#include <cppunit/TestFixture.h>
#include <cppunit/TestSuite.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestCaller.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/ui/text/TestRunner.h>

using namespace std::chrono;

typedef unsigned long long u64;


/* test_push_pop_worker */

template<typename item_type, typename queue_type>
struct test_push_pop_worker
{
    typedef std::vector<item_type> vec_type;
    
    vec_type vec;
    queue_type &queue;
    const size_t items_per_thread;
    std::thread thread;
    
    test_push_pop_worker(queue_type &queue, const size_t items_per_thread)
        : queue(queue), items_per_thread(items_per_thread), thread(&test_push_pop_worker::mainloop, this) {}
    
    void mainloop()
    {
        // transfer items from the queue to the vector
        for (size_t i = 0; i < items_per_thread; i++) {
            item_type v = queue.pop_front();
            if (v) {
                vec.push_back(v);
            } else {
                log_debug("%p queue.pop_front() returned null item", std::this_thread::get_id());
            }
        }
        // transfer items from vector to the queue
        for (auto v : vec) {
            if (!queue.push_back(v)) {
                log_debug("%p queue.push_back() returned false", std::this_thread::get_id());
            }
        }
    }
};

/* test_push_pop_threads */

template<typename item_type, typename queue_type>
void test_push_pop_threads(const char* queue_type_name, const size_t num_threads, const size_t iterations, const size_t items_per_thread)
{
    const size_t num_items = num_threads * items_per_thread;
    const size_t num_ops = num_items * iterations;
    
    typedef test_push_pop_worker<item_type, queue_type> worker_type;
    typedef std::shared_ptr<worker_type> worker_ptr;
    typedef std::vector<worker_ptr> worker_list;
    typedef std::set<item_type> set_type;
    
    queue_type queue(num_items);
    
    // populate queue
    CPPUNIT_ASSERT(queue.size() == 0);
    for (size_t i = 1; i <= num_items; i++) {
        queue.push_back(item_type(i));
    }
    CPPUNIT_ASSERT(queue.size() == num_items);
    
    // run test iterations
    const auto t1 = std::chrono::high_resolution_clock::now();
    for (size_t iter = 0; iter < iterations; iter++)
    {
        // start worker threads
        worker_list workers;
        for (size_t i = 0; i < num_threads; i++) {
            workers.push_back(std::make_shared<worker_type>(queue, items_per_thread));
        }
        
        // join worker threads
        for (auto worker : workers) {
            worker->thread.join();
        }
        CPPUNIT_ASSERT(queue.size() == num_items);
    }
    const auto t2 = std::chrono::high_resolution_clock::now();
    uint64_t work_time_us = duration_cast<microseconds>(t2 - t1).count();
    
    // transfer items to a set
    set_type check_set;
    for (size_t i = 1; i <= num_items; i++) {
        item_type v = queue.pop_front();
        if (v) {
            check_set.insert(v);
        } else {
            log_debug("queue.pop_front() returned null item");
        }
    }
    CPPUNIT_ASSERT(queue.size() == 0);
    
    // check items in set
    size_t check_count = 0;
    for (size_t i = 1; i <= num_items; i++) {
        if (check_set.find(item_type(i)) != check_set.end()) {
            check_count++;
        }
    }
    CPPUNIT_ASSERT(check_count == num_items);
    
    printf("test_push_pop_threads::%-25s num_threads=%-3lu iterations=%-5lu items_per_thread=%-9lu "
           "time(µs)=%-9llu ops=%-9llu op_time(µs)=%-9.6lf\n",
            queue_type_name, num_threads, iterations, items_per_thread,
            (u64)work_time_us, (u64)num_ops, (double)work_time_us / (double)num_ops);
}


/* test_queue */

class test_queue : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(test_queue);
    CPPUNIT_TEST(test_queue_constants);
    CPPUNIT_TEST(test_empty_invariants);
    CPPUNIT_TEST(test_push_pop);
    CPPUNIT_TEST(test_threads);
    CPPUNIT_TEST_SUITE_END();

public:
    
    void setUp() {}
    void tearDown() {}
    
    void test_queue_constants()
    {
        const size_t qsize = 1024;
        typedef queue_atomic<void*> qtype;
        qtype q(qsize);
        
        printf("queue_atomic::is_lock_free  = %u\n", q.counter_back.is_lock_free());
        printf("queue_atomic::atomic_bits   = %u\n", qtype::atomic_bits);
        printf("queue_atomic::offset_bits   = %u\n", qtype::offset_bits);
        printf("queue_atomic::version_bits  = %u\n", qtype::version_bits);
        printf("queue_atomic::offset_shift  = %u\n", qtype::offset_shift);
        printf("queue_atomic::version_shift = %u\n", qtype::version_shift);
        printf("queue_atomic::size_max      = 0x%016llx (%llu)\n", (u64)qtype::size_max, (u64)qtype::size_max);
        printf("queue_atomic::offset_limit  = 0x%016llx (%llu)\n", (u64)qtype::offset_limit, (u64)qtype::offset_limit);
        printf("queue_atomic::version_limit = 0x%016llx (%llu)\n", (u64)qtype::version_limit, (u64)qtype::version_limit);
        printf("queue_atomic::offset_mask   = 0x%016llx\n", (u64)qtype::offset_mask);
        printf("queue_atomic::version_mask  = 0x%016llx\n", (u64)qtype::version_mask);
        
        assert(qtype::atomic_bits   == 64);
        assert(qtype::offset_bits   == 48);
        assert(qtype::version_bits  == 16);
        assert(qtype::offset_shift  == 0);
        assert(qtype::version_shift == 48);
        assert(qtype::size_max      == 140737488355328ULL);
        assert(qtype::offset_limit  == 281474976710656ULL);
        assert(qtype::version_limit == 65536);
        assert(qtype::offset_mask   == 0x0000ffffffffffffULL);
        assert(qtype::version_mask  == 0x000000000000ffffULL);
    }
    
    void test_empty_invariants()
    {
        const size_t qsize = 1024;
        typedef queue_atomic<void*> qtype;
        qtype q(qsize);
        
        assert(q.capacity() == 1024);
        assert(q.size() == 0);
        assert(q.empty() == true);
        assert(q.full() == false);
        assert(q.size_limit == 1024);
        assert(q._back_version() == 0);
        assert(q._front_version() == 0);
        assert(q._back() == 0);
        assert(q._front() == 1024);
    }
    
    void test_push_pop()
    {
        const size_t qsize = 4;
        typedef queue_atomic<void*> qtype;
        qtype q(qsize);
        
        // check initial invariants
        assert(q.capacity() == qsize);
        assert(q.size() == 0);
        assert(q.empty() == true);
        assert(q.full() == false);
        assert(q.size_limit == qsize);
        assert(q._back_version() == 0);
        assert(q._front_version() == 0);
        assert(q._back() == 0);
        assert(q._front() == qsize);
        
        // push_back 4 items
        for (size_t i = 1; i <= 4; i++) {
            assert(q.push_back((void*)i) == true);
            assert(q._back_version() == i);
            assert(q._front_version() == 0);
            assert(q._back() == i);
            assert(q._front() == qsize);
            assert(q.size() == i);
            assert(q.empty() == false);
            assert(q.full() == (i < 4 ? false : true));
        }
        
        // push_back overflow test
        assert(q.push_back((void*)5) == false);
        assert(q._back_version() == 4);
        assert(q._front_version() == 0);
        assert(q._back() == 4);
        assert(q._front() == qsize);
        assert(q.size() == 4);
        assert(q.empty() == false);
        assert(q.full() == true);
        
        // pop_front 4 items
        for (size_t i = 1; i <= 4; i++) {
            assert(q.pop_front() == (void*)i);
            assert(q._back_version() == 4);
            assert(q._front_version() == i);
            assert(q._back() == 4);
            assert(q._front() == 4 + i);
            assert(q.size() == 4 - i);
            assert(q.empty() == (i > 3 ? true : false));
            assert(q.full() == false);
        }
        
        // pop_front underflow test
        assert(q.pop_front() == (void*)0);
        assert(q._back_version() == 4);
        assert(q._front_version() == 4);
        assert(q._back() == 4);
        assert(q._front() == 8);
        assert(q.size() == 0);
        assert(q.empty() == true);
        assert(q.full() == false);
        
        // push_back 4 items
        for (size_t i = 1; i <= 4; i++) {
            assert(q.push_back((void*)i) == true);
            assert(q._back_version() == 4 + i);
            assert(q._front_version() == 4);
            assert(q._back() == 4 + i);
            assert(q._front() == 8);
            assert(q.size() == i);
            assert(q.empty() == false);
            assert(q.full() == (i < 4 ? false : true));
        }
        
        // push_back overflow test
        assert(q.push_back((void*)5) == false);
        assert(q._back_version() == 8);
        assert(q._front_version() == 4);
        assert(q._back() == 8);
        assert(q._front() == 8);
        assert(q.size() == 4);
        assert(q.empty() == false);
        assert(q.full() == true);
        
        // pop_front 4 items
        for (size_t i = 1; i <= 4; i++) {
            assert(q.pop_front() == (void*)i);
            assert(q._back_version() == 8);
            assert(q._front_version() == 4 + i);
            assert(q._back() == 8);
            assert(q._front() == 8 + i);
            assert(q.size() == 4 - i);
            assert(q.empty() == (i > 3 ? true : false));
            assert(q.full() == false);
        }
        
        // pop_front underflow test
        assert(q.pop_front() == (void*)0);
        assert(q._back_version() == 8);
        assert(q._front_version() == 8);
        assert(q._back() == 8);
        assert(q._front() == 12);
        assert(q.size() == 0);
        assert(q.empty() == true);
        assert(q.full() == false);
    }
    
    void test_threads()
    {
        test_push_pop_threads<int,queue_atomic<int>>("queue_atomic", 8, 10, 1024);
        test_push_pop_threads<int,queue_atomic<int>>("queue_atomic", 8, 10, 65536);
        test_push_pop_threads<int,queue_atomic<int>>("queue_atomic", 8, 64, 65536);
        test_push_pop_threads<int,queue_atomic<int>>("queue_atomic", 8, 16, 262144);
    }
};

int main(int argc, const char * argv[])
{
    CppUnit::TestResult controller;
    CppUnit::TestResultCollector result;
    CppUnit::TextUi::TestRunner runner;
    CppUnit::CompilerOutputter outputer(&result, std::cerr);
    
    controller.addListener(&result);
    runner.addTest(test_queue::suite());
    runner.run(controller);
    outputer.write();
}

