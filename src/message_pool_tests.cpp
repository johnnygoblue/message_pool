#include "message_pool.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <random>

using namespace std::chrono_literals;

TEST(MessagePoolTest, BasicFunctionality) {
    MessagePool pool(10);

    // Test initial state
    EXPECT_EQ(pool.capacity(), 10);
    EXPECT_EQ(pool.available(), 10);

    // Borrow and release single message
    auto* msg1 = pool.borrow();
    EXPECT_NE(msg1, nullptr);
    EXPECT_EQ(pool.available(), 9);

    pool.release(msg1);
    EXPECT_EQ(pool.available(), 10);
}

TEST(MessagePoolTest, ExhaustPool) {
    MessagePool pool(5);
    std::vector<NetworkMessage*> messages;

    // Borrow all messages (should succeed)
    for (size_t i = 0; i < pool.capacity(); ++i) {
        messages.push_back(pool.borrow());
        EXPECT_NE(messages.back(), nullptr);
    }

    EXPECT_EQ(pool.available(), 0);

    // Next borrow should fail with timeout
    EXPECT_THROW({
        try {
            pool.borrow();
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "Timeout waiting for available message");
            throw;
        }
    }, std::runtime_error);

    // Release one message and borrow again (should succeed)
    pool.release(messages.back());
    messages.pop_back();
    EXPECT_NO_THROW({
        auto* msg = pool.borrow();
        EXPECT_NE(msg, nullptr);
        pool.release(msg);
    });

    // Release all messages
    for (auto* msg : messages) {
        pool.release(msg);
    }
    EXPECT_EQ(pool.available(), pool.capacity());
}

TEST(MessagePoolTest, MessageReuse) {
    MessagePool pool(3);
    std::vector<int> seenIds;

    // Borrow and release messages multiple times
    for (int i = 0; i < 10; ++i) {
        auto* msg = pool.borrow();
        seenIds.push_back(msg->id);
        pool.release(msg);
    }

    // Should only see IDs 0, 1, 2 being reused
    for (auto id : seenIds) {
        EXPECT_GE(id, 0);
        EXPECT_LT(id, 3);
    }
}

TEST(MessagePoolTest, InvalidRelease) {
    MessagePool pool(2);
    NetworkMessage invalidMsg;
    invalidMsg.id = -1;

    EXPECT_THROW(pool.release(&invalidMsg), std::runtime_error);

    invalidMsg.id = 2; // Out of bounds
    EXPECT_THROW(pool.release(&invalidMsg), std::runtime_error);

    invalidMsg.id = 0;
    EXPECT_NO_THROW(pool.release(&invalidMsg)); // Actually in pool
}

TEST(MessagePoolTest, ThreadSafety) {
    constexpr size_t POOL_SIZE = 5;  // Small pool to force contention
    constexpr size_t THREAD_COUNT = 20;
    constexpr size_t ITERATIONS = 1000;

    MessagePool pool(POOL_SIZE);
    std::vector<std::thread> threads;
    std::atomic<size_t> active_borrows{0};
    std::atomic<bool> stop_flag{false};
    std::atomic<size_t> total_collisions{0};
    std::atomic<size_t> max_concurrent_borrows{0};

    // Contention monitor thread
    std::thread monitor([&]() {
        while(!stop_flag) {
            size_t current = active_borrows.load();
            max_concurrent_borrows = std::max(max_concurrent_borrows.load(), current);
            if(current > 1) {
                total_collisions++;
            }
            //std::this_thread::sleep_for(1ms);
            std::this_thread::sleep_for(1ms);
        }
    });

    for (size_t i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> work_dist(100, 1000); // 100-1000μs

            for (size_t j = 0; j < ITERATIONS; ++j) {
                try {
                    auto* msg = pool.borrow();
                    active_borrows++;

                    // Simulate variable work
                    std::this_thread::sleep_for(
                        std::chrono::microseconds(work_dist(gen)));

                    active_borrows--;
                    pool.release(msg);
                } catch (...) {}
            }
        });
    }

    for (auto& t : threads) t.join();
    stop_flag = true;
    monitor.join();

    // Record and display metrics
    RecordProperty("TotalCollisions", total_collisions.load());
    RecordProperty("MaxConcurrentBorrows", max_concurrent_borrows.load());

    std::cout << "\nThread Safety Metrics:\n"
              << "  Total collisions: " << total_collisions << "\n"
              << "  Max concurrent borrows: " << max_concurrent_borrows << "\n"
              << "  Final pool available: " << pool.available() << "/" << POOL_SIZE << "\n";

    EXPECT_GT(total_collisions, 0) << "No thread collisions detected - test not valid";
    EXPECT_EQ(pool.available(), POOL_SIZE) << "Pool leak detected";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
