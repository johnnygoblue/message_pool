#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <chrono>

struct NetworkMessage {
    int id;         // Used by pool for tracking
    char data[256]; // Actual message payload
};

class MessagePool {
public:
    explicit MessagePool(size_t poolSize, std::chrono::milliseconds timeout = std::chrono::milliseconds(100))
        : poolSize_(poolSize), timeout_(timeout) {
        storage_.reserve(poolSize_);
        for (size_t i = 0; i < poolSize_; ++i) {
            auto msg = std::make_unique<NetworkMessage>();
            msg->id = static_cast<int>(i);
            storage_.push_back(std::move(msg));
            freeList_.push_back(i);
        }
    }

    NetworkMessage* borrow() {
        std::unique_lock<std::mutex> lock(mutex_);

        // Wait until a message becomes available
        if (!cv_.wait_for(lock, timeout_, [this]() { return !freeList_.empty(); })) {
            throw std::runtime_error("Timeout waiting for available message");
        }

        size_t index = freeList_.front();
        freeList_.erase(freeList_.begin());
        return storage_[index].get();
    }

    void release(NetworkMessage* msg) {
        if (!msg) return;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (msg->id < 0 || static_cast<size_t>(msg->id) >= poolSize_) {
                throw std::runtime_error("Invalid message ID");
            }
            freeList_.push_back(static_cast<size_t>(msg->id));
        }

        cv_.notify_one();
    }

    size_t available() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return freeList_.size();
    }

    size_t capacity() const { return poolSize_; }

private:
    size_t poolSize_;
    std::vector<std::unique_ptr<NetworkMessage>> storage_;
    std::vector<size_t> freeList_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::chrono::milliseconds timeout_;
};
