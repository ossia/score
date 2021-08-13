#include <Scenario/Application/Drops/DropProcessOnState.hpp>

#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>

#include <QApplication>

namespace Scenario
{

class DropProcessOnStateHelper
{
public:
  DropProcessOnStateHelper(
      const StateModel& sourceState,
      const Scenario::ProcessModel& scenar,
      const score::DocumentContext& ctx,
      TimeVal maxdur)
      : m_sequence{bool(qApp->keyboardModifiers() & Qt::ShiftModifier)}
      , m_scenar{scenar}
      , m_macro{new Command::AddProcessInNewBoxMacro, ctx}
  {
    auto& m = m_macro;

    const auto& parent_ev = Scenario::parentEvent(sourceState, scenar);
    const auto& date = parent_ev.date();
    m_currentDate = date;
    if (!m_sequence)
    {
      if (!sourceState.nextInterval())
      {
        m_intervalY = sourceState.heightPercentage();
        // Everything will go in a single interval
        m_itv = &m.createIntervalAfter(
            scenar,
            sourceState.id(),
            Scenario::Point{date + maxdur, m_intervalY});
      }
      else
      {
        m_intervalY = sourceState.heightPercentage() + 0.1;
        m_createdState
            = m.createState(scenar, parent_ev.id(), m_intervalY).id();
        m_itv = &m.createIntervalAfter(
            scenar,
            m_createdState,
            Scenario::Point{date + maxdur, m_intervalY});
      }
    }
    else
    {
      if (!sourceState.nextInterval())
      {
        m_intervalY = sourceState.heightPercentage();
        m_createdState = sourceState.id();
      }
      else
      {
        m_intervalY = sourceState.heightPercentage() + 0.1;
        m_createdState
            = m.createState(scenar, parent_ev.id(), m_intervalY).id();
      }
    }
  }

  template <typename F>
  Process::ProcessModel* addProcess(F&& fun, TimeVal duration)
  {
    // sequence : processes are put all one after the other
    if (m_sequence)
    {
      {
        // We create the first interval / process
        m_currentDate += duration;
        m_itv = &m_macro.createIntervalAfter(
            m_scenar,
            m_createdState,
            Scenario::Point{m_currentDate, m_intervalY});
        decltype(auto) proc = fun(m_macro, *m_itv);
        m_macro.showRack(*m_itv);
        return proc;
      }
    }
    else
    {
      SCORE_ASSERT(m_itv);
      return fun(m_macro, *m_itv);
    }
  }

  void commit()
  {
    if (!m_sequence)
    {
      SCORE_ASSERT(m_itv);
      m_macro.showRack(*m_itv);
    }

    m_macro.commit();
  }

  Scenario::IntervalModel& interval() { return *m_itv; }
  Scenario::Command::Macro& macro() { return m_macro; }

private:
  const bool m_sequence{};
  double m_intervalY{};
  TimeVal m_currentDate{};
  const Scenario::ProcessModel& m_scenar;
  QPointF m_pos;
  Scenario::Command::Macro m_macro;
  Scenario::IntervalModel* m_itv{};
  Id<StateModel> m_createdState;
};

bool DropProcessOnState::drop(
    const StateModel& st,
    const ProcessModel& scenar,
    const QMimeData& mime,
    const score::DocumentContext& ctx)
{

  const auto& handlers = ctx.app.interfaces<Process::ProcessDropHandlerList>();

  if (auto res = handlers.getDrop(mime, ctx); !res.empty())
  {
    auto t = handlers.getMaxDuration(res).value_or(TimeVal::fromMsecs(10000.));

    DropProcessOnStateHelper dropper(st, scenar, ctx, t);

    score::Dispatcher_T disp{dropper.macro()};
    for (const auto& proc : res)
    {
      Process::ProcessModel* p = dropper.addProcess(
          [&](Scenario::Command::Macro& m,
              const IntervalModel& itv) -> Process::ProcessModel* {
            auto p = m.createProcessInNewSlot(
                itv, proc.creation.key, proc.creation.customData, {});
            if (auto& name = proc.creation.prettyName; !name.isEmpty())
              dropper.macro().submit(
                  new Scenario::Command::ChangeElementName{*p, name});
            return p;
          },
          proc.duration ? *proc.duration : t);
      if (p && proc.setup)
      {
        proc.setup(*p, disp);
      }
    }

    if (res.size() == 1)
    {
      const auto& name = res.front().creation.prettyName;
      auto& itv = dropper.interval();
      if (!name.isEmpty())
      {
        dropper.macro().submit(
            new Scenario::Command::ChangeElementName{itv, name});
      }
    }
    dropper.commit();
    return true;
  }
  return true;
}

}
