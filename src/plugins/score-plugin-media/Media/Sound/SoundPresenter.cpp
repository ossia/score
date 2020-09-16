#include "SoundPresenter.hpp"

#include "SoundView.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Media/Commands/ChangeAudioFile.hpp>
#include <Media/Sound/Drop/SoundDrop.hpp>
#include <Media/Tempo.hpp>

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
    updateTempo();
    m_view->recompute(m_ratio);
  });

  m_view->setData(layer.file());
  updateTempo();
  m_view->recompute(m_ratio);

  connect(m_view, &LayerView::askContextMenu, this, &LayerPresenter::contextMenuRequested);
  connect(m_view, &LayerView::dropReceived, this, &LayerPresenter::onDrop);
  con(layer, &Sound::ProcessModel::nativeTempoChanged,
      this, &LayerPresenter::updateTempo);
  con(layer, &Sound::ProcessModel::scoreTempoChanged,
      this, &LayerPresenter::updateTempo);
  con(layer, &Sound::ProcessModel::stretchModeChanged,
      this, &LayerPresenter::updateTempo);
}

void LayerPresenter::updateTempo()
{
  const auto& layer = (const ProcessModel&) m_process;
  if(layer.stretchMode() == ossia::audio_stretch_mode::None)
  {
    m_view->setTempoRatio(1.);
    return;
  }

  const double tempoAtStart = Media::tempoAtStartDate(m_process);
  if(tempoAtStart < 0.1)
    return;

  const double nativeTempo = layer.nativeTempo();
  if(nativeTempo < 0.1)
    return;

  m_view->setTempoRatio(tempoAtStart / nativeTempo);
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
