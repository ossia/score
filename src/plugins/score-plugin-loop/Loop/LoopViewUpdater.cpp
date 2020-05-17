// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "LoopViewUpdater.hpp"

#include <Loop/LoopPresenter.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Loop/LoopView.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/State/StateView.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncView.hpp>
#include <Scenario/Document/VerticalExtent.hpp>

namespace Loop
{
ViewUpdater::ViewUpdater(LayerPresenter& presenter) : m_presenter{presenter} { }

void ViewUpdater::updateEvent(const Scenario::EventPresenter& event)
{
  event.view()->setExtent(extent());

  event.view()->setPos({event.model().date().toPixels(m_presenter.m_zoomRatio), extent().top()});

  // We also have to move all the relevant states
  if (&event == m_presenter.m_startEventPresenter)
  {
    updateState(*m_presenter.m_startStatePresenter);
  }
  else if (&event == m_presenter.m_endEventPresenter)
  {
    updateState(*m_presenter.m_endStatePresenter);
  }

  m_presenter.m_view->update();
}

void ViewUpdater::updateInterval(const Scenario::TemporalIntervalPresenter& pres)
{
  auto msPerPixel = m_presenter.m_zoomRatio;

  const auto& cstr_model = pres.model();
  Scenario::TemporalIntervalView& cstr_view = Scenario::view(pres);

  auto startPos = cstr_model.date().toPixels(msPerPixel);
  auto delta = cstr_view.x() - startPos;
  bool dateChanged = (delta * delta > 1); // Magnetism

  if (dateChanged)
  {
    cstr_view.setPos({startPos, extent().top()});
  }
  else
  {
    cstr_view.setY(extent().top());
  }

  cstr_view.setDefaultWidth(cstr_model.duration.defaultDuration().toPixels(msPerPixel));
  cstr_view.setMinWidth(cstr_model.duration.minDuration().toPixels(msPerPixel));
  cstr_view.setMaxWidth(
      cstr_model.duration.isMaxInfinite(),
      cstr_model.duration.isMaxInfinite()
          ? -1
          : cstr_model.duration.maxDuration().toPixels(msPerPixel));

  m_presenter.m_view->update();
}

void ViewUpdater::updateTimeSync(const Scenario::TimeSyncPresenter& timesync)
{
  timesync.view()->setExtent(2. * extent());

  timesync.view()->setPos(
      {timesync.model().date().toPixels(m_presenter.m_zoomRatio), extent().top()});

  m_presenter.m_view->update();
}

void ViewUpdater::updateState(const Scenario::StatePresenter& state)
{
  if (&state == m_presenter.m_startStatePresenter)
  {
    const auto& ev = m_presenter.model().startEvent();
    state.view()->setPos({ev.date().toPixels(m_presenter.m_zoomRatio), extent().top()});
  }
  else if (&state == m_presenter.m_endStatePresenter)
  {
    const auto& ev = m_presenter.model().endEvent();
    state.view()->setPos({ev.date().toPixels(m_presenter.m_zoomRatio), extent().top()});
  }

  m_presenter.m_view->update();
}
}
