#pragma once
#include <Gfx/Graph/RenderState.hpp>

#include <score_plugin_gfx_export.h>

#include <private/qrhi_p.h>

namespace score::gfx
{

/**
 * @brief Copies `count` elements between two GPU buffers with different
 * strides, in one compute dispatch.
 *
 * The transfer equivalent needs one copy region per element: tens of
 * thousands of regions per frame for an instanced scene, which the CPU has to
 * record (and the validation layer to check) every frame.
 *
 * Both buffers are bound as storage buffers and copied as 32-bit words, so
 * every offset, stride and the element size are multiples of 4: see
 * eligible().
 *
 * Usage: init() once per QRhi, prepare() per operation, updateParams() with a
 * live batch before the compute pass, dispatch() inside it.
 */
class SCORE_PLUGIN_GFX_TEST_EXPORT GPUStridedCopy
{
public:
  struct Params
  {
    QRhiBuffer* src{};
    QRhiBuffer* dst{};
    uint32_t count{};             // number of elements
    uint32_t element_bytes{};     // bytes copied per element
    uint32_t src_offset_bytes{};
    uint32_t src_stride_bytes{};
    uint32_t dst_offset_bytes{};
    uint32_t dst_stride_bytes{};
  };

  struct PreparedOp
  {
    QRhiShaderResourceBindings* srb{};
    QRhiBuffer* paramsUBO{};
    // The buffers srb was last created against, by globalResourceId: a buffer
    // freed and reallocated can come back at the same address.
    quint64 src_id{};
    quint64 dst_id{};
  };

  //! Whether the copy can run on this shader: storage-capable, non-dynamic
  //! buffers, word-aligned layout, in bounds.
  static bool eligible(const Params& p) noexcept;

  bool init(RenderState& state);
  void release();

  PreparedOp prepare(QRhi& rhi, const Params& p);
  static void releaseOp(PreparedOp& op);

  void updateParams(QRhiResourceUpdateBatch& res, PreparedOp& op, const Params& p);
  void dispatch(QRhiCommandBuffer& cb, const PreparedOp& op, const Params& p);

  static constexpr int LocalSize = 256;

private:
  struct Dims
  {
    int x{}, y{};
  };
  Dims dims(uint64_t invocations) const noexcept;

  QRhiComputePipeline* m_pipeline{};
  QShader m_shader;
  int m_maxWorkgroupsPerDim{65535};
};

}
