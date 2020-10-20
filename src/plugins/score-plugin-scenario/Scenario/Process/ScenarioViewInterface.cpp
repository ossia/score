// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioViewInterface.hpp"

#include "ScenarioPresenter.hpp"

#include <Process/TimeValue.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockPresenter.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockView.hpp>
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
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioView.hpp>

#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>

namespace Scenario
{
ScenarioViewInterface::ScenarioViewInterface(const ScenarioPresenter& presenter)
    : m_presenter{presenter}
{
}

void ScenarioViewInterface::on_eventMoved(const EventPresenter& ev)
{
  auto h = m_presenter.m_view->boundingRect().height();

  ev.view()->setExtent(ev.extent() * h);

  ev.view()->setPos({ev.model().date().toPixels(m_presenter.m_zoomRatio), ev.extent().top() * h});

  // We also have to move all the relevant states
  for (const auto& state : ev.model().states())
  {
    auto state_it = m_presenter.m_states.find(state);
    if (state_it != m_presenter.m_states.end())
    {
      on_stateMoved(*state_it);
    }
  }
  m_presenter.m_view->update();
}

void ScenarioViewInterface::on_intervalMoved(const TemporalIntervalPresenter& pres)
{
  auto rect = m_presenter.m_view->boundingRect();
  auto msPerPixel = m_presenter.m_zoomRatio;

  const auto& cstr_model = pres.model();
  auto& cstr_view = view(pres);

  double startPos = cstr_model.date().toPixels(msPerPixel);
  // double delta = cstr_view.x() - startPos;
  bool dateChanged = true; // Disabled because it does a whacky movement when
                           // there are processes. (delta * delta > 1); //
                           // Magnetism

  if (dateChanged)
  {
    cstr_view.setPos(startPos, std::round(rect.height() * cstr_model.heightPercentage()));
  }
  else
  {
    cstr_view.setY(std::round(rect.height() * cstr_model.heightPercentage()));
  }

  cstr_view.setDefaultWidth(cstr_model.duration.defaultDuration().toPixels(msPerPixel));
  cstr_view.setMinWidth(cstr_model.duration.minDuration().toPixels(msPerPixel));
  cstr_view.setMaxWidth(
      cstr_model.duration.isMaxInfinite(),
      cstr_model.duration.isMaxInfinite()
          ? -1
          : cstr_model.duration.maxDuration().toPixels(msPerPixel));
  cstr_view.setRigid(cstr_model.duration.isRigid());

  pres.updateAllSlots();
  m_presenter.m_view->update();
}

void ScenarioViewInterface::on_timeSyncMoved(const TimeSyncPresenter& timesync)
{
  auto h = m_presenter.m_view->boundingRect().height();
  timesync.view()->setExtent(timesync.extent() * h);

  timesync.view()->setPos(
      {timesync.model().date().toPixels(m_presenter.m_zoomRatio), timesync.extent().top() * h});

  m_presenter.m_view->update();
}

void ScenarioViewInterface::on_stateMoved(const StatePresenter& state)
{
  auto rect = m_presenter.m_view->boundingRect();
  const auto& ev = m_presenter.model().event(state.model().eventId());

  state.view()->setPos(
      {ev.date().toPixels(m_presenter.m_zoomRatio),
       rect.height() * state.model().heightPercentage()});

  m_presenter.m_view->update();
}

void ScenarioViewInterface::on_commentMoved(const CommentBlockPresenter& comment)
{
  auto h = m_presenter.m_view->boundingRect().height();
  comment.view()->setPos(
      comment.date().toPixels(m_presenter.zoomRatio()), comment.model().heightPercentage() * h);
  m_presenter.m_view->update();
}

template <typename T>
void update_min_max(const T& val, T& min, T& max)
{
  min = val < min ? val : min;
  max = val > max ? val : max;
}

void ScenarioViewInterface::on_graphicalScaleChanged(double scale)
{
  for (auto& e : m_presenter.getEvents())
  {
    e.view()->setWidthScale(scale);
  }
  for (auto& s : m_presenter.getStates())
  {
    s.view()->setScale(scale);
  }

  m_presenter.m_view->update();
}
}
