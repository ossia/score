#pragma once

#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>

#include <score/tools/std/IndirectContainer.hpp>

template <typename Model_T, typename IntervalPresenter_T>
class BaseScenarioPresenter
{
public:
  BaseScenarioPresenter(const Model_T& model) : m_model{model} { }

  virtual ~BaseScenarioPresenter() = default;

  score::IndirectContainer<IntervalPresenter_T> getIntervals() const
  {
    return {m_intervalPresenter};
  }

  score::IndirectContainer<Scenario::StatePresenter> getStates() const
  {
    return {m_startStatePresenter, m_endStatePresenter};
  }

  score::IndirectContainer<Scenario::EventPresenter> getEvents() const
  {
    return {m_startEventPresenter, m_endEventPresenter};
  }

  score::IndirectContainer<Scenario::TimeSyncPresenter> getTimeSyncs() const
  {
    return {m_startNodePresenter, m_endNodePresenter};
  }

  const Scenario::EventPresenter& event(const Id<Scenario::EventModel>& id) const
  {
    if (id == m_model.startEvent().id())
      return *m_startEventPresenter;
    else if (id == m_model.endEvent().id())
      return *m_endEventPresenter;
    SCORE_ABORT;
  }
  const Scenario::TimeSyncPresenter& timeSync(const Id<Scenario::TimeSyncModel>& id) const
  {
    if (id == m_model.startTimeSync().id())
      return *m_startNodePresenter;
    else if (id == m_model.endTimeSync().id())
      return *m_endNodePresenter;
    SCORE_ABORT;
  }
  const IntervalPresenter_T& interval(const Id<Scenario::IntervalModel>& id) const
  {
    if (id == m_model.interval().id())
      return *m_intervalPresenter;
    SCORE_ABORT;
  }
  const Scenario::StatePresenter& state(const Id<Scenario::StateModel>& id) const
  {
    if (id == m_model.startState().id())
      return *m_startStatePresenter;
    else if (id == m_model.endState().id())
      return *m_endStatePresenter;
    SCORE_ABORT;
  }

  const Scenario::TimeSyncModel& startTimeSync() const { return m_startNodePresenter->model(); }

  IntervalPresenter_T* intervalPresenter() const { return m_intervalPresenter; }

protected:
  const Model_T& m_model;

  IntervalPresenter_T* m_intervalPresenter{};
  Scenario::StatePresenter* m_startStatePresenter{};
  Scenario::StatePresenter* m_endStatePresenter{};
  Scenario::EventPresenter* m_startEventPresenter{};
  Scenario::EventPresenter* m_endEventPresenter{};
  Scenario::TimeSyncPresenter* m_startNodePresenter{};
  Scenario::TimeSyncPresenter* m_endNodePresenter{};
};
