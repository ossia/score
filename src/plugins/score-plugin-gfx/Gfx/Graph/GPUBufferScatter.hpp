#pragma once
#include <Gfx/Graph/RenderState.hpp>

#include <private/qrhi_p.h>

namespace score::gfx
{

/**
 * @brief GPU-side buffer format conversion (scatter).
 *
 * Converts vertex attribute data between formats on the GPU using a compute
 * shader. The typical use case is widening float3→float4 when uploading
 * CPU mesh data into SSBOs for compute shader consumption.
 *
 * The raw CPU data is uploaded as-is into a staging SSBO (bulk memcpy, no
 * per-element processing). A small compute dispatch then reads from the
 * staging buffer and writes to the destination SSBO with proper format
 * conversion and default component values (GPU convention: x=0,y=0,z=0,w=1).
 *
 * Usage:
 *   1. Call init() once per QRhi lifetime.
 *   2. In the update phase: upload raw CPU data to staging, call updateParams().
 *   3. In the render phase: call dispatch() inside a compute pass.
 */
class GPUBufferScatter
{
public:
  struct Params
  {
    QRhiBuffer* staging{};    // Source SSBO (raw CPU data, uploaded as-is)
    QRhiBuffer* output{};     // Destination SSBO (format-converted result)
    uint32_t element_count{};
    uint32_t src_components{}; // Floats per source element (1-4)
    uint32_t dst_components{}; // Floats per destination element (1-4)
    uint32_t src_stride_floats{}; // Stride between source elements (in floats, >= src_components)
    uint32_t src_offset_floats{}; // Starting offset in source buffer (in floats)
  };

  bool init(RenderState& state);
  void release();

  /// Create or update an SRB+UBO for a specific scatter operation.
  struct PreparedOp
  {
    QRhiShaderResourceBindings* srb{};
    QRhiBuffer* paramsUBO{};
  };
  PreparedOp prepare(QRhi& rhi, const Params& p);

  /// Update params UBO and SRB. Call BEFORE beginComputePass (needs a live batch).
  void updateParams(QRhiResourceUpdateBatch& res, const PreparedOp& op, const Params& p);

  /// Dispatch the scatter. Must be called inside beginComputePass/endComputePass.
  void dispatch(QRhiCommandBuffer& cb, const PreparedOp& op, const Params& p);

  static constexpr int LocalSize = 256;

  /// Dispatch grid dimensions, in workgroups. When the required workgroup
  /// count exceeds the backend's per-dimension limit the count is spread
  /// across the Y (and Z) axes, mirroring RenderedCSFNode's 1D_BUFFER clamp.
  struct DispatchDims
  {
    int x{};
    int y{};
    int z{};
  };

private:
  /// Compute the (clamped) dispatch grid for @p element_count elements at
  /// LocalSize threads per workgroup, spreading across Y/Z so no axis exceeds
  /// m_maxWorkgroupsPerDim. Used by both updateParams() (to populate the UBO)
  /// and dispatch() (to issue the dispatch) so the two always agree.
  DispatchDims computeDispatchDims(uint32_t element_count) const;

  QRhiComputePipeline* m_pipeline{};
  QShader m_shader;
  int m_maxWorkgroupsPerDim{65535};
};

} // namespace score::gfx
