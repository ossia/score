#pragma once
#include <Gfx/Widgets/TexturePreview.hpp>

#include <ossia/gfx/port_index.hpp>

#include <QPointer>
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
using port_index = ossia::gfx::port_index;

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
class SCORE_PLUGIN_GFX_TEST_EXPORT RhiPreviewWidget : public QWidget
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

  /// Context backend. The producer endpoint can be updated at any time; the
  /// widget rewires the preview edge accordingly. It is a (node, port) pair,
  /// not just a node: a process whose texture outlet is not its first outlet
  /// registers under that outlet's own index.
  void useContext(GfxContext* ctx, port_index producer);
  void setProducer(port_index producer);

  //! The attached node of the Graph backend, or null once its owner is gone.
  score::gfx::BackgroundNode* liveNode() const noexcept;
  //! The preview of the Context backend.
  const TexturePreview& contextPreview() const noexcept { return m_preview; }

  //! Stop rendering and hand the BackgroundNode back. Idempotent, and leaves
  //! the widget reusable through useGraph()/useContext(). Callers that own the
  //! graph must detach before destroying it: this widget is a child of
  //! something that may outlive it.
  void detach();

protected:
  void paintEvent(QPaintEvent* ev) override;
  void resizeEvent(QResizeEvent* ev) override;
  void timerEvent(QTimerEvent* ev) override;

private:
  void attach();

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

  // Context backend: the preview output and its readback
  TexturePreview m_preview;

  // Graph backend
  std::shared_ptr<QRhiReadbackResult> m_readback;
  score::gfx::BackgroundNode* m_node{};  // owned by m_graph after attach

  int m_timerId{};
};
}
