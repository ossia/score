#pragma once
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

namespace Scenario
{

template<typename T>
class DropProcessInScenario
{
public:
  DropProcessInScenario(const Scenario::ScenarioPresenter& pres, QPointF pos, TimeVal maxdur)
    : m_sequence{bool(qApp->keyboardModifiers() & Qt::ShiftModifier)}
    , m_pres{pres}
    , m_pos{pos}
    , m_macro{new T, pres.context().context}
  {
    if(!m_sequence)
    {
      const Scenario::ProcessModel& scenar = pres.model();
      Scenario::Point pt = pres.toScenarioPoint(pos);
      m_itv = &m_macro.createBox(scenar, pt.date, pt.date + maxdur, pt.y);
    }
  }

  template<typename F>
  void addProcess(F&& fun, TimeVal duration)
  {
    if(m_sequence)
    {
      const Scenario::ProcessModel& scenar = m_pres.model();
      Scenario::Point pt = m_pres.toScenarioPoint(m_pos);
      if(m_itv)
      {
        auto last_state = m_itv->endState();
        pt.date = Scenario::parentEvent(scenar.state(last_state), scenar).date() + duration;
        m_itv = &m_macro.createIntervalAfter(scenar, last_state, pt);
        auto& proc = fun(m_macro, *m_itv);
        m_macro.addLayerInNewSlot(*m_itv, proc);
        m_macro.showRack(*m_itv);
      }
      else
      {
        m_itv = &m_macro.createBox(scenar, pt.date, pt.date + duration, pt.y);
        auto& proc = fun(m_macro, *m_itv);
        m_macro.addLayerInNewSlot(*m_itv, proc);
        m_macro.showRack(*m_itv);
      }
    }
    else
    {
      auto& proc = fun(m_macro, *m_itv);
      m_macro.addLayerInNewSlot(*m_itv, proc);
    }
  }

  void commit()
  {
    if(!m_sequence)
      m_macro.showRack(*m_itv);

    m_macro.commit();
  }

private:
  const bool m_sequence;
  const Scenario::ScenarioPresenter& m_pres;
  QPointF m_pos;
  Scenario::Command::Macro m_macro;
  Scenario::IntervalModel* m_itv{};
};


// TODO
// TODO use me in SoundDrop
template<typename T>
class DropProcessInInterval
{
public:
  DropProcessInInterval(const Scenario::IntervalModel& interval)
  {

  }

  template<typename F>
  void addProcess(F&& fun, TimeVal duration)
  {
  }

  void commit()
  {
  }

private:
};
}
