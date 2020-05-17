#include "SoundPresenter.hpp"

#include "SoundView.hpp"

#include <Media/Commands/ChangeAudioFile.hpp>
#include <Media/Sound/Drop/SoundDrop.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

namespace Media
{
namespace Sound
{
LayerPresenter::LayerPresenter(
    const ProcessModel& layer,
    LayerView* view,
    const Process::Context& ctx,
    QObject* parent)
    : Process::LayerPresenter{layer, view, ctx, parent}, m_view{view}
{
  connect(
      view, &LayerView::pressed, this, [&]() { m_context.context.focusDispatcher.focus(this); });

  con(layer, &ProcessModel::fileChanged, this, [&]() {
    m_view->setData(layer.file());
    m_view->recompute(m_ratio);
  });

  m_view->setData(layer.file());
  m_view->recompute(m_ratio);

  connect(m_view, &LayerView::askContextMenu, this, &LayerPresenter::contextMenuRequested);
  connect(m_view, &LayerView::dropReceived, this, &LayerPresenter::onDrop);
}

void LayerPresenter::setWidth(qreal val, qreal defaultWidth)
{
  m_view->setWidth(val);
}

void LayerPresenter::setHeight(qreal val)
{
  m_view->setHeight(val);
}

void LayerPresenter::putToFront()
{
  m_view->recompute(m_ratio);
  m_view->setFrontColors(true);
}

void LayerPresenter::putBehind()
{
  m_view->setFrontColors(false);
}

void LayerPresenter::on_zoomRatioChanged(ZoomRatio r)
{
  m_ratio = r;
  m_view->recompute(m_ratio);
}

void LayerPresenter::parentGeometryChanged()
{
  m_view->recompute(m_ratio);
}

void LayerPresenter::onDrop(const QPointF& p, const QMimeData& mime)
{
  DroppedAudioFiles drops{context().context, mime};
  if (!drops.valid() || drops.files.size() != 1)
  {
    return;
  }
  CommandDispatcher<> disp{context().context.commandStack};
  disp.submit<Media::ChangeAudioFile>(
      static_cast<const Sound::ProcessModel&>(m_process), std::move(drops.files.front().first));
}
}
}
