#include "app/Runtime.h"
#include <SDL2/SDL.h>
#include <gtest/gtest.h>

using namespace arcanee;
using namespace arcanee::app;
using namespace arcanee::input;

class DeterminismReplayTest : public ::testing::Test {
protected:
  void SetUp() override {
    // SDL might be initialized already by other tests, or not at all.
    // Runtime handles subsystems.
  }

  void TearDown() override {
    // Clear any pushed events?
    SDL_Event e;
    while (SDL_PollEvent(&e))
      ;
  }
};

TEST_F(DeterminismReplayTest, ReplayProducesSameHash) {
  // 1. Setup Headless Runtime
  // We need a dummy cartridge path. VfsImpl expects it?
  // VfsImpl checks if cart exists if we try to load it.
  // But Runtime calls loadCartridge. If it fails, it might log error but
  // continue? Let's use "samples/hello" if available, or create a dummy file.
  // Actually, VfsImpl usually mounts the directory.
  // We can pass a dummy path if we don't rely on cartridge script logic for
  // this test (input hash is part of sim state hash).

  Runtime::Config config1;
  config1.cartridgePath = "test_cart";
  Runtime runtime(config1);
  // If loading fails, loop still runs but update is empty. InputManager still
  // updates. Valid enough for input replay test.

  auto *input = runtime.getInputManager();
  ASSERT_NE(input, nullptr);

  // 2. Record
  input->startRecording();

  // Run for 30 ticks
  for (int i = 0; i < 30; ++i) {
    // Inject input
    if (i == 10) {
      // User presses SPACE
      // Since we can't easily inject into SDL queue reliably in headless
      // without window sometimes? Actually SDL_PushEvent works without window
      // for SDL_INIT_EVENTS.
      SDL_Event e;
      e.type = SDL_KEYDOWN;
      e.key.keysym.scancode = SDL_SCANCODE_SPACE;
      SDL_PushEvent(&e);
    }
    /*
    if (i == 20) {
      SDL_Event e;
      e.type = SDL_KEYUP;
      e.key.keysym.scancode = SDL_SCANCODE_SPACE;
      SDL_PushEvent(&e);
    }
    */

    runtime.runHeadless(1);
  }

  input->stopRecording();
  auto recorded = input->getRecordedData();
  u64 hashA = runtime.getSimStateHash();

  // Verify we recorded something
  ASSERT_EQ(recorded.size(), 30);
  // Verify SPACE was pressed in snapshot at index 11 (event processed in tick
  // 11?) Actually: i=10: Push Event. runHeadless(1) -> InputManager::update()
  // -> PollEvent -> Process -> liveState updated -> frozen to current. So
  // snapshot 10 should have space? Or 11? Let's check logic. But strictly, we
  // care about Replay matching.

  // 3. Replay
  Runtime::Config config2;
  config2.cartridgePath = "test_cart";
  Runtime runtime2(config2);
  auto *input2 = runtime2.getInputManager();

  input2->startPlayback(recorded);

  runtime2.runHeadless(30);

  u64 hashB = runtime2.getSimStateHash();

  // 4. Verification settings
  EXPECT_EQ(hashA, hashB) << "Sim state hash should be identical after replay";

  // Also verify that hash is NOT zero (meaning something happened)
  // Input hashing adds some values.
  EXPECT_NE(hashA, 0);
}
