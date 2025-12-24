#include "audio/AudioQueue.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace arcanee::audio;

TEST(AudioQueueTest, PushPopWorks) {
  SPSCQueue<int, 4> queue;

  // Empty initially
  EXPECT_TRUE(queue.empty());

  int val = 0;
  EXPECT_FALSE(queue.pop(val));

  // Push
  EXPECT_TRUE(queue.push(1));
  EXPECT_FALSE(queue.empty());

  EXPECT_TRUE(queue.push(2));
  EXPECT_TRUE(queue.push(3));

  // Capacity is 4, but due to ring buffer logic usually capacity-1 is usable to
  // distinguish full/empty? Let's check implementation. nextWrite = (write + 1)
  // % Capacity. if nextWrite == read return false. So capacity-1 items can be
  // stored. 4 -> 3 items.

  EXPECT_FALSE(queue.push(4)); // Should fail (full)

  // Pop
  EXPECT_TRUE(queue.pop(val));
  EXPECT_EQ(val, 1);

  // Now can push again
  EXPECT_TRUE(queue.push(4));

  EXPECT_TRUE(queue.pop(val));
  EXPECT_EQ(val, 2);
  EXPECT_TRUE(queue.pop(val));
  EXPECT_EQ(val, 3);
  EXPECT_TRUE(queue.pop(val));
  EXPECT_EQ(val, 4);

  EXPECT_TRUE(queue.empty());
}

TEST(AudioQueueTest, ThreadedProducerConsumer) {
  SPSCQueue<uint32_t, 128> queue;
  std::atomic<bool> done{false};
  std::vector<uint32_t> consumed;

  std::thread consumer([&]() {
    uint32_t val;
    while (!done || !queue.empty()) {
      if (queue.pop(val)) {
        consumed.push_back(val);
      } else {
        std::this_thread::yield();
      }
    }
  });

  // Producer (Main Thread)
  for (uint32_t i = 0; i < 100; ++i) {
    while (!queue.push(i)) {
      std::this_thread::yield();
    }
  }

  done = true;
  consumer.join();

  EXPECT_EQ(consumed.size(), 100);
  for (uint32_t i = 0; i < 100; ++i) {
    EXPECT_EQ(consumed[i], i);
  }
}
