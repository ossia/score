#pragma once
#include <score_plugin_gfx_export.h>

#include <QtGlobal>

class QRhi;
class QRhiBuffer;
class QRhiCommandBuffer;
class QRhiComputePipeline;
class QRhiResourceUpdateBatch;
class QRhiShaderResourceBindings;

namespace score::gfx
{
struct RenderState;

/**
 * @brief The draw-count rung, emulated with compute for backends without it.
 *
 * A producer that writes its draw count on the GPU may leave anything in the
 * command slots past that count: syn-pop-draw writes poison commands there.
 * The count rung (drawIndexedIndirectCount) never reads them, but it is not
 * always there: Metal implements it with indirect command buffers, which a
 * pipeline that binds textures cannot use, and some drivers lack it outright.
 * The full-capacity multi-draw rung would then draw the dead slots.
 *
 * run() copies the commands to a buffer of its own with every slot at or past
 * the count zeroed, so a multi-draw of the copy paints what the count draw
 * would. Nothing is read back. It needs compute and a storage-usable command
 * and count buffer, and returns nullptr otherwise.
 *
 * Call it outside a pass, before the render pass that draws.
 */
class SCORE_PLUGIN_GFX_TEST_EXPORT IndirectCountClamp
{
public:
  struct Input
  {
    QRhiBuffer* commands{};
    quint32 commandsOffset{}; // bytes, a multiple of 4
    quint32 stride{};         // bytes, a multiple of 4
    quint32 slots{};          // command slots to copy
    QRhiBuffer* count{};
    quint32 countOffset{}; // bytes, a multiple of 4
    //! RenderList::frame: run() refuses a second run in the same frame, whose
    //! parameters would overwrite the first's in their per-frame uniform
    //! buffer before either dispatch executes. -1: unchecked.
    int64_t frame{-1};
  };

  /// The clamped copy of the commands, at offset 0 with the same stride, or
  /// nullptr when the clamp cannot run (the caller keeps its plain rungs).
  QRhiBuffer* run(
      RenderState& state, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res,
      const Input& in);

  void release();

  //! Why the last run() returned nullptr, or nullptr when it did not.
  const char* refusal() const noexcept { return m_refusal; }

  static constexpr int LocalSize = 64;

  //! For tests: the resource ids of the bindings the dispatch uses and of the
  //! ones the pipeline was last given (ids, not addresses: a new object can
  //! take a deleted one's address).
  quint64 bindingsId() const noexcept;
  quint64 pipelineBindingsId() const noexcept { return m_pipelineBindingsId; }

private:
  bool ensurePipeline(RenderState& state);

  QRhiComputePipeline* m_pipeline{};
  QRhiShaderResourceBindings* m_srb{};
  QRhiBuffer* m_params{};
  QRhiBuffer* m_clamped{};
  bool m_failed{};
  const char* m_refusal{};
  int64_t m_lastFrame{-1};
  quint64 m_pipelineBindingsId{};

  // What m_srb binds; any change rebuilds it.
  quint64 m_boundCommands{};
  quint64 m_boundCount{};
  quint32 m_boundCommandsSize{};
  quint32 m_boundCountSize{};
};
}
