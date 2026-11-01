#pragma once
#include <Effect/EffectLayer.hpp>

#include <Gfx/Widgets/TexturePreview.hpp>

#include <QImage>
#include <QString>
#include <QPointer>

#include <functional>
#include <optional>

#include <score_plugin_gfx_export.h>

namespace Gfx
{
/**
 * The view of a process that shows what its own texture outlet produces.
 *
 * While the view is shown in a graphics view, a preview output follows the
 * outlet. Being an output of the graph, the preview pulls the process, which
 * then renders whether or not its outlet is cabled further.
 */
class SCORE_PLUGIN_GFX_EXPORT OutletPreviewLayer : public Process::EffectLayerView
{
public:
  OutletPreviewLayer(
      const Process::ProcessModel& process, const Process::Context& doc,
      QGraphicsItem* parent, std::size_t texture_outlet = 0);
  ~OutletPreviewLayer() override;

  //! Width over height of the image; nothing: fill the view.
  std::function<std::optional<double>()> aspect;
  //! Shown when there is no image yet.
  QString placeholder;

  //! Where the image is drawn: as large as fits, centred.
  QRectF imageRect() const;
  const TexturePreview& preview() const noexcept { return m_preview; }

protected:
  void paint_impl(QPainter* p) const override;
  void timerEvent(QTimerEvent* ev) override;
  //! Drawn over the image.
  virtual void paintOverlay(QPainter* p, const QRectF& image) const { }

private:
  void refresh();
  //! The polling rate: fast while previewing, slow while waiting for the
  //! process to run, none when previews are disabled.
  void updateTimer();

  const Process::ProcessModel& m_process;
  std::size_t m_outlet{};
  QPointer<GfxContext> m_context;
  TexturePreview m_preview;
  int m_timer{};
  int m_interval{-1};
};
}
