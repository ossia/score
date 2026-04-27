// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SequencePresenter.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>
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
  // Update handles + sections when model structure changes
  connect(
      &model, &SequenceModel::structureChanged, this,
      [this] {
        rebuildSections();
        updateHandles();
      });

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

  // Build section presenters for initial model state.
  // Zoom hasn't been set yet so we defer layout to on_zoomRatioChanged.
  rebuildSections();
}

SequencePresenter::~SequencePresenter()
{
  qDeleteAll(m_sectionPresenters);
}

void SequencePresenter::setWidth(qreal width, qreal defaultWidth)
{
  m_view.setWidth(width);
  updateHandles();
  updateSectionLayout();
}

void SequencePresenter::setHeight(qreal height)
{
  m_view.setHeight(height);
  // Child section presenters manage their own height based on slot content;
  // we only propagate the full height so each one knows the available space.
  for(auto* p : m_sectionPresenters)
    p->view()->setHeight(height);
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
  for(auto* p : m_sectionPresenters)
    p->on_zoomRatioChanged(ratio);
  updateHandles();
  updateSectionLayout();
}

void SequencePresenter::parentGeometryChanged()
{
  updateHandles();
  updateSectionLayout();
}

void SequencePresenter::rebuildSections()
{
  qDeleteAll(m_sectionPresenters);
  m_sectionPresenters.clear();

  const auto& startTsId = m_model.startTimeSyncId();
  const auto& endTsId = m_model.endTimeSyncId();

  // Use ordered intervals so section presenters are created left-to-right,
  // although position is set by date so order only affects zValue stacking.
  for(const auto& itvId : m_model.orderedIntervals())
  {
    const auto& itv = m_model.intervals.at(itvId);

    // Section intervals span from one boundary IS to the next.
    // All are non-boundary by construction (boundary ISes are the
    // start/end timeSyncs — section intervals connect intermediate ISes
    // or the boundaries themselves, but never skip them).
    auto* pres = new Scenario::TemporalIntervalPresenter{
        m_zoom, itv, m_context, false, &m_view, this};
    pres->on_zoomRatioChanged(m_zoom);
    m_sectionPresenters.append(pres);
  }

  updateSectionLayout();
}

void SequencePresenter::updateSectionLayout()
{
  if(m_zoom <= 0)
    return;

  for(auto* pres : m_sectionPresenters)
  {
    const auto& itv = pres->model();
    const double x = itv.date().toPixels(m_zoom);
    const double w = itv.duration.defaultDuration().toPixels(m_zoom);
    pres->view()->setPos(x, 0.0);
    pres->view()->setDefaultWidth(w);
    pres->view()->setMinWidth(itv.duration.minDuration().toPixels(m_zoom));
    pres->view()->setMaxWidth(
        itv.duration.isMaxInfinite(),
        itv.duration.isMaxInfinite() ? -1.
                                     : itv.duration.maxDuration().toPixels(m_zoom));
  }
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
