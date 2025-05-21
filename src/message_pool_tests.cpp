#include "message_pool.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

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
    constexpr size_t POOL_SIZE = 100;
    constexpr size_t THREAD_COUNT = 20;  // Increased thread count
    constexpr size_t ITERATIONS = 10000; // More iterations

    MessagePool pool(POOL_SIZE);
    std::vector<std::thread> threads;
    std::atomic<size_t> borrowCount{0};
    std::atomic<size_t> errors{0};

    for (size_t i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&pool, &borrowCount, &errors]() {
            for (size_t j = 0; j < ITERATIONS; ++j) {
                try {
                    auto* msg = pool.borrow();
                    borrowCount++;

                    // Verify message is valid
                    if (msg->id < 0 || static_cast<size_t>(msg->id) >= pool.capacity()) {
                        errors++;
                    }

                    // Simulate work (random delay)
                    std::this_thread::sleep_for(
                        std::chrono::microseconds(1 + (rand() % 10)));

                    pool.release(msg);
                } catch (const std::exception& e) {
                    errors++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors, 0) << "Encountered " << errors << " thread safety errors";
    EXPECT_EQ(pool.available(), POOL_SIZE)
        << "Pool should return to full capacity after all threads complete";
    EXPECT_EQ(borrowCount, THREAD_COUNT * ITERATIONS)
        << "All borrow operations should complete successfully";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
