#pragma once
#include <Gfx/Graph/RenderState.hpp>

#include <QtGui/private/qrhi_p.h>

#include <Ui/Monitor/BufferGather.hpp>

namespace Ui::Monitor::Buffer
{
/**
 * Records the gather pass of a GatherPlan and the readback of its result.
 *
 * The staging buffer grows to what the largest readback needed and is kept;
 * so are the pipeline and the bindings while the source and the range it
 * reads stay the same.
 */
class GatherPass
{
public:
  GatherPass() = default;
  GatherPass(const GatherPass&) = delete;
  GatherPass& operator=(const GatherPass&) = delete;
  ~GatherPass();

  //! Whether the pass can read `source` on `rhi`.
  Fallback check(QRhi& rhi, const QRhiBuffer& source) const noexcept;

  /**
   * Records the pass, outside any other pass of `cb`, and the readback of
   * its result into `out`. `res` holds the updates to apply first; the pass
   * takes it and leaves it null.
   *
   * Returns false, with nothing recorded and `res` left as it was, when the
   * resources of the pass could not be created.
   */
  bool record(
      score::gfx::RenderState& state, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res,
      QRhiBuffer& source, const GatherPlan& plan, QRhiBufferReadbackResult& out);

  //! Deletes the resources. A readback recorded must have completed.
  void release() noexcept;

private:
  bool ensurePipeline(score::gfx::RenderState& state);
  bool ensureStaging(QRhi& rhi, int64_t bytes);
  bool ensureBindings(QRhi& rhi, QRhiBuffer& source, const GatherPlan& plan);

  QShader m_shader;
  QRhiComputePipeline* m_pipeline{};
  QRhiShaderResourceBindings* m_srb{};
  QRhiBuffer* m_params{};
  QRhiBuffer* m_staging{};
  //! What the bindings were created for, by globalResourceId: a buffer freed
  //! and created again can come back at the same address
  quint64 m_sourceId{};
  quint64 m_stagingId{};
  int64_t m_bindOffset{-1};
  int64_t m_bindSize{-1};
  bool m_failed{};
};
}
