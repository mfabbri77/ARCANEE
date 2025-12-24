#include "render/RenderDevice.h"
#include <gtest/gtest.h>

using namespace arcanee;

class RenderDeviceSmokeTest : public ::testing::Test {
protected:
  void SetUp() override { m_device = std::make_unique<render::RenderDevice>(); }

  void TearDown() override { m_device.reset(); }

  std::unique_ptr<render::RenderDevice> m_device;
};

TEST_F(RenderDeviceSmokeTest, InitialStateIsUnknown) {
  EXPECT_EQ(m_device->getBackend(),
            render::RenderDevice::RenderBackend::Unknown);
}

TEST_F(RenderDeviceSmokeTest, HandlesInvalidWindowGracefully) {
  // Pass invalid X11 handles (0)
  // This expects the implementation to return false rather than crash.
  // Note: Diligent might crash inside CreateSwapChainVk if we are unlucky.
  // But robust engine code should try to catch that or validation?
  // Actually, standard X11 calls might Segfault if display is null.
  // RenderDevice.cpp:
  // pFactoryVk->CreateSwapChainVk(..., window, ...)
  // window.pDisplay = static_cast<Display*>(displayHandle);

  // If Diligent dereferences pDisplay, it will crash.
  // Let's verify if we crash. If we do, we should add a check in
  // RenderDevice.cpp!

  bool result = m_device->initialize(nullptr, 0);
  EXPECT_FALSE(result);
  EXPECT_EQ(m_device->getBackend(),
            render::RenderDevice::RenderBackend::Unknown);
}
