#pragma once

#include "common/Types.h"
#include <functional>
#include <string>
#include <vector>

namespace arcanee::render {

/**
 * @brief Render pass types in ARCANEE.
 *
 * Execution order per spec §5.7:
 * 1. Clear CBUF
 * 2. 3D Scene (gfx3d.*)
 * 3. 2D Canvas (gfx.*)
 * 4. Present to swapchain
 */
enum class RenderPassType : u8 {
  Clear,    // Clear CBUF to background color
  Scene3D,  // 3D scene rendering (future)
  Canvas2D, // ThorVG 2D canvas
  Present   // Present CBUF to swapchain
};

/**
 * @brief A single render pass in the graph.
 */
struct RenderPass {
  std::string name;
  RenderPassType type;
  std::function<void()> execute;
  bool enabled = true;
};

/**
 * @brief Simple render pass scheduler.
 *
 * Manages ordered execution of render passes each frame.
 * Pass order: Clear → Scene3D → Canvas2D → Present
 */
class RenderGraph {
public:
  RenderGraph() = default;
  ~RenderGraph() = default;

  /**
   * @brief Add a pass to the graph.
   */
  void addPass(const std::string &name, RenderPassType type,
               std::function<void()> fn) {
    m_passes.push_back({name, type, std::move(fn), true});
  }

  /**
   * @brief Enable/disable a pass by name.
   */
  void setPassEnabled(const std::string &name, bool enabled) {
    for (auto &pass : m_passes) {
      if (pass.name == name) {
        pass.enabled = enabled;
        break;
      }
    }
  }

  /**
   * @brief Execute all enabled passes in order.
   */
  void execute() {
    // Sort by type to ensure correct order
    for (auto &pass : m_passes) {
      if (pass.enabled && pass.execute) {
        pass.execute();
      }
    }
  }

  void clear() { m_passes.clear(); }

  size_t passCount() const { return m_passes.size(); }

private:
  std::vector<RenderPass> m_passes;
};

} // namespace arcanee::render
