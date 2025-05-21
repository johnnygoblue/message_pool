# Thread-Safe Message Pool

A high-performance, thread-safe memory pool for network message objects, designed for low-latency applications like trading systems.

## Features

- **Thread-safe** borrowing/releasing of messages
- **Timeout support** for pool exhaustion
- **FIFO behavior** for predictable performance
- **Contiguous memory** for cache efficiency
- **Zero dynamic allocations** during operation

## Prerequisites

- CMake ≥ 3.10
- C++17 compatible compiler
- Google Test (gtest)
- Make or Ninja

### Ubuntu/Debian
```bash
sudo apt install build-essential cmake libgtest-dev
```

## File Structure
```
message_pool/
├── include/
│   └── message_pool.h    # Main pool implementation
├── src/
│   └── message_pool_tests.cpp  # Test cases
├── CMakeLists.txt        # Build configuration
└── README.md            # This file
```

## Building from Source

### Linux/macOS

```bash
# 1. Clone the repository
git clone https://github.com/yourusername/message_pool.git
cd message_pool

# 2. Configure build system (creates 'build' directory)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# 3. Compile the project
cmake --build build --config Release --parallel $(nproc)

# 4. Run tests (optional)
cd build && ctest --output-on-failure -j$(nproc)
```

## Example Usage
```cpp
#include "message_pool.h"

void process(NetworkMessage* msg);

int main() {
    // Create pool with 100 messages
    MessagePool pool(100);
    
    try {
        // Borrow message (50ms timeout)
        auto* msg = pool.borrow(std::chrono::milliseconds(50));
        
        // Use message
        process(msg);
        
        // Return to pool
        pool.release(msg);
    } 
    catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
```

## Reference
- Inspired by [Masoud Farsi's LinkedIn post](https://www.linkedin.com/posts/bardifarsi_memorymanagement-cpp-objectpooling-activity-7329673529300254720-SF9g?utm_source=share&utm_medium=member_desktop&rcm=ACoAAAnmHlIBYiMh15lgd_IkUUR5YGzapqtTvfU)