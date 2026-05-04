#pragma once
#include <QWidget>

#include <score_plugin_gfx_export.h>

#include <cstdint>
#include <functional>
#include <memory>

struct QRhiReadbackResult;

namespace score::gfx
{
struct Graph;
struct BackgroundNode;
}

namespace Gfx
{
class GfxContext;

/**
 * @brief A QWidget that paints a score::gfx render-graph output without using
 *        QWidget::createWindowContainer (broken on macOS) or QRhiWidget
 *        (forces the toplevel to switch to RHI compositing — flash + perf
 *        impact).
 *
 * The graph renders into an offscreen QRhi texture owned by a
 * score::gfx::BackgroundNode; each frame is read back into a QImage-shaped
 * QByteArray and drawn in paintEvent. CPU readback is cheap at preview
 * resolutions and avoids touching Qt's compositor RHI entirely.
 *
 * Two backends:
 *   - Graph backend (useGraph): caller owns a score::gfx::Graph and drives
 *     wiring through callbacks. Used by ShaderPreviewManager.
 *   - Context backend (useContext): caller routes registration through a
 *     Gfx::GfxContext. The GfxContext's manual timer drives the offscreen
 *     render; the widget only triggers QWidget::update() to refresh the
 *     painted image. Used by GraphPreviewWidget (texture-port preview).
 */
class SCORE_PLUGIN_GFX_EXPORT RhiPreviewWidget : public QWidget
{
public:
  explicit RhiPreviewWidget(QWidget* parent = nullptr);
  ~RhiPreviewWidget() override;

  /// Graph backend. onAttached fires once the BackgroundNode has been
  /// registered with the graph (its render list is built). The caller wires
  /// producer→preview edges in there. onAboutToDetach fires before the
  /// BackgroundNode is removed; the caller must remove any edges it added.
  void useGraph(
      score::gfx::Graph* graph,
      std::function<void(score::gfx::BackgroundNode&)> onAttached,
      std::function<void(score::gfx::BackgroundNode&)> onAboutToDetach);

  /// Context backend. The producer node id can be updated at any time; the
  /// widget rewires the preview edge accordingly.
  void useContext(GfxContext* ctx, int32_t producerNodeId);
  void setProducerNodeId(int32_t id);

protected:
  void paintEvent(QPaintEvent* ev) override;
  void resizeEvent(QResizeEvent* ev) override;
  void timerEvent(QTimerEvent* ev) override;

private:
  void attach();
  void detach();

  enum class Backend
  {
    None,
    Graph,
    Context
  } m_backend{Backend::None};

  // Graph backend
  score::gfx::Graph* m_graph{};
  std::function<void(score::gfx::BackgroundNode&)> m_onAttached;
  std::function<void(score::gfx::BackgroundNode&)> m_onAboutToDetach;

  // Context backend
  GfxContext* m_ctx{};
  int32_t m_producerNodeId{-1};
  int32_t m_screenNodeId{-1};
  bool m_edgeConnected{false};

  std::shared_ptr<QRhiReadbackResult> m_readback;
  score::gfx::BackgroundNode* m_node{};  // owned by m_graph or m_ctx after attach
  int m_timerId{};
};
}
