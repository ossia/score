#pragma once
#include <Gfx/Graph/RenderState.hpp>

#include <score_plugin_gfx_export.h>

#include <private/qrhi_p.h>

namespace score::gfx
{

/**
 * @brief Widens 16-bit indices in a GPU buffer into 32-bit ones in another.
 *
 * A buffer copy cannot do it where blits need 4-byte-aligned offsets and
 * sizes (Metal): the source is read as 32-bit words, and each index lands in
 * its own uint32 slot of the destination.
 *
 * Usage: init() once per QRhi, prepare() per operation, updateParams() with a
 * live batch before the compute pass, dispatch() inside it.
 */
class SCORE_PLUGIN_GFX_EXPORT GPUIndexWiden
{
public:
  struct Params
  {
    QRhiBuffer* src{};          // uint16 indices, bound as a storage buffer
    QRhiBuffer* dst{};          // uint32 indices, bound as a storage buffer
    uint32_t count{};           // number of indices
    uint32_t src_offset_bytes{}; // a multiple of 2
    uint32_t dst_offset_bytes{}; // a multiple of 4
  };

  struct PreparedOp
  {
    QRhiShaderResourceBindings* srb{};
    QRhiBuffer* paramsUBO{};
  };

  bool init(RenderState& state);
  void release();

  PreparedOp prepare(QRhi& rhi, const Params& p);
  static void releaseOp(PreparedOp& op);

  void updateParams(QRhiResourceUpdateBatch& res, const PreparedOp& op, const Params& p);
  void dispatch(QRhiCommandBuffer& cb, const PreparedOp& op, const Params& p);

  static constexpr int LocalSize = 256;

private:
  struct Dims
  {
    int x{}, y{};
  };
  Dims dims(uint32_t count) const noexcept;

  QRhiComputePipeline* m_pipeline{};
  QShader m_shader;
  int m_maxWorkgroupsPerDim{65535};
};

}
