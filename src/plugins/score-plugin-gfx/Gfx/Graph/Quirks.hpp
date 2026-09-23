#pragma once
#include <score_plugin_gfx_export.h>

class QRhi;

namespace score::gfx
{
/**
 * @brief Defects of the driver in use, to be worked around.
 *
 * The counterpart of RenderState::Caps, which holds what the backend supports.
 * One per RenderList: the answers depend on the QRhi backend and the physical
 * device, neither of which changes while a render list lives.
 *
 * SCORE_GFX_NO_QUIRKS=1 leaves every flag unset.
 */
struct SCORE_PLUGIN_GFX_EXPORT Quirks
{
  /**
   * @brief An empty render pass below the sample count of an earlier
   * rasterising pass in the same command buffer loses the device.
   *
   * NVIDIA + Vulkan, Xid 69 class 0xc797, on the 595 and 615 branches. Equal
   * or rising sample counts are fine, and so is the same pair of passes split
   * over two command buffers.
   */
  bool emptyPassSampleDescentLosesDevice{};

  void populate(QRhi& rhi);
};
}
