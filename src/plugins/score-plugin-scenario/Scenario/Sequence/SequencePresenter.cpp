// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SequencePresenter.hpp"

#include <Scenario/Sequence/Commands/MoveSequenceIS.hpp>

#include <Process/TimeValue.hpp>

#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Sequence::SequencePresenter)

namespace Sequence
{

SequencePresenter::SequencePresenter(
    const SequenceModel& model, SequenceView* view, const Process::Context& ctx,
    QObject* parent)
    : Process::LayerPresenter{model, view, ctx, parent}
    , m_model{model}
    , m_view{*view}
{
  // Update handles when model structure changes
  connect(&model, &SequenceModel::structureChanged, this, &SequencePresenter::updateHandles);

  // Handle drag: dispatch ongoing MoveSequenceIS command
  connect(
      view, &SequenceView::handleDragMoved, this,
      [this](Id<Scenario::TimeSyncModel> tsId, double newX) {
        if(m_zoom <= 0)
          return;
        const auto newDate = TimeVal::fromPixels(newX, m_zoom);
        m_context.context.dispatcher.submit<Sequence::Command::MoveSequenceIS>(
            m_model, tsId, newDate);
      });

  connect(
      view, &SequenceView::handleDragReleased, this,
      [this](Id<Scenario::TimeSyncModel>, double) {
        m_context.context.dispatcher.commit();
      });

  connect(view, &SequenceView::handleDragCancelled, this, [this]() {
    m_context.context.dispatcher.rollback();
    updateHandles();
  });
}

SequencePresenter::~SequencePresenter() = default;

void SequencePresenter::setWidth(qreal width, qreal defaultWidth)
{
  m_view.setWidth(width);
  updateHandles();
}

void SequencePresenter::setHeight(qreal height)
{
  m_view.setHeight(height);
}

void SequencePresenter::putToFront()
{
  m_view.setVisible(true);
}

void SequencePresenter::putBehind()
{
  m_view.setVisible(false);
}

void SequencePresenter::on_zoomRatioChanged(ZoomRatio ratio)
{
  m_zoom = ratio;
  updateHandles();
}

void SequencePresenter::parentGeometryChanged()
{
  updateHandles();
}

void SequencePresenter::updateHandles()
{
  if(m_zoom <= 0)
    return;

  const auto& startTsId = m_model.startTimeSyncId();
  const auto& endTsId = m_model.endTimeSyncId();

  QVector<SequenceView::HandleData> handles;
  for(const auto& ts : m_model.timeSyncs)
  {
    // Skip boundary timeSyncs — they map to the parent scenario's states
    if(ts.id() == startTsId || ts.id() == endTsId)
      continue;
    const double x = ts.date().toPixelsRaw(m_zoom);
    handles.push_back({ts.id(), x});
  }

  m_view.setHandles(handles);
}

} // namespace Sequence
