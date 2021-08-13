#include <Scenario/Application/Drops/DropProcessInScenario.hpp>

#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>

#include <QApplication>

namespace Scenario
{

class DropProcessInScenarioHelper
{
public:
  DropProcessInScenarioHelper(
      MagneticStates m_magnetic,
      const Scenario::ScenarioPresenter& pres,
      QPointF pos,
      TimeVal maxdur)
      : m_sequence{bool(qApp->keyboardModifiers() & Qt::ShiftModifier)}
      , m_pres{pres}
      , m_pos{pos}
      , m_macro{new Command::AddProcessInNewBoxMacro, pres.context().context}
  {
    auto& m = m_macro;
    const auto& scenar = pres.model();
    Scenario::Point pt = pres.toScenarioPoint(pos);

    auto [x_state, y_state, magnetic] = m_magnetic;
    if (y_state)
    {
      m_intervalY = pt.y;
      if (magnetic || pt.date <= scenar.event(y_state->eventId()).date())
      {
        // Create another state on that event and put the process afterwards
        m_createdState
            = m.createState(scenar, y_state->eventId(), m_intervalY).id();
      }
      else
      {
        auto& s = m.createState(scenar, y_state->eventId(), m_intervalY);
        auto& i
            = m.createIntervalAfter(scenar, s.id(), {pt.date, m_intervalY});
        m_createdState = i.endState();
      }
    }
    else if (x_state)
    {
      if (x_state->nextInterval())
      {
        // We create from the event instead
        m_intervalY = pt.y;
        auto& s = m.createState(scenar, x_state->eventId(), m_intervalY);
        auto& i
            = m.createIntervalAfter(scenar, s.id(), {pt.date, m_intervalY});
        m_createdState = i.endState();
      }
      else
      {
        m_intervalY = magnetic ? x_state->heightPercentage() : pt.y;

        auto& i = m.createIntervalAfter(
            scenar, x_state->id(), {pt.date, m_intervalY});
        m_createdState = i.endState();
      }
    }
    else
    {
      // We create in the emptiness
      const auto& [t, e, s] = m.createDot(scenar, pt);
      m_createdState = s.id();
    }

    if (!m_sequence)
    {
      // Everything will go in a single interval
      m_itv = &m.createIntervalAfter(
          scenar,
          m_createdState,
          Scenario::Point{pt.date + maxdur, m_intervalY});
    }
  }

  template <typename F>
  Process::ProcessModel* addProcess(F&& fun, TimeVal duration)
  {
    // sequence : processes are put all one after the other
    if (m_sequence)
    {
      const Scenario::ProcessModel& scenar = m_pres.model();
      Scenario::Point pt = m_pres.toScenarioPoint(m_pos);
      if (m_itv)
      {
        // We already created the first interval / process
        auto last_state = m_itv->endState();
        pt.date
            = Scenario::parentEvent(scenar.state(last_state), scenar).date()
              + duration;
        m_itv = &m_macro.createIntervalAfter(
            scenar, last_state, {pt.date, m_intervalY});
        decltype(auto) proc = fun(m_macro, *m_itv);
        m_macro.showRack(*m_itv);
        return proc;
      }
      else
      {
        // We create the first interval / process
        m_itv = &m_macro.createIntervalAfter(scenar, m_createdState, {pt.date + duration, pt.y});
        decltype(auto) proc = fun(m_macro, *m_itv);
        m_macro.showRack(*m_itv);
        return proc;
      }
    }
    else
    {
      return fun(m_macro, *m_itv);
    }
  }

  void commit()
  {
    if (!m_sequence)
      m_macro.showRack(*m_itv);

    m_macro.commit();
  }

  Scenario::IntervalModel& interval() { return *m_itv; }
  Scenario::Command::Macro& macro() { return m_macro; }

private:
  const bool m_sequence{};
  double m_intervalY{};
  const Scenario::ScenarioPresenter& m_pres;
  QPointF m_pos;
  Scenario::Command::Macro m_macro;
  Scenario::IntervalModel* m_itv{};
  Id<StateModel> m_createdState;
};


DropProcessInScenario::DropProcessInScenario() { }

void DropProcessInScenario::init()
{
  const auto& handlers
      = score::GUIAppContext().interfaces<Process::ProcessDropHandlerList>();
  for (auto& handler : handlers)
  {
    for (auto& type : handler.mimeTypes())
      m_acceptableMimeTypes.push_back(type);
    for (auto& ext : handler.fileExtensions())
      m_acceptableSuffixes.push_back(ext);
  }
}
bool DropProcessInScenario::drop(
    const ScenarioPresenter& pres,
    QPointF pos,
    const QMimeData& mime)
{
  const auto& ctx = pres.context().context;
  const auto& handlers = ctx.app.interfaces<Process::ProcessDropHandlerList>();

  if (auto res = handlers.getDrop(mime, ctx); !res.empty())
  {
    auto t = handlers.getMaxDuration(res).value_or(TimeVal::fromMsecs(10000.));
    DropProcessInScenarioHelper dropper(m_magnetic, pres, pos, t);

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
  return false;
}
}
