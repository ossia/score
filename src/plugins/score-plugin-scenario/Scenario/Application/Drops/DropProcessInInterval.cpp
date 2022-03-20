#include <Scenario/Application/Drops/DropProcessInInterval.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/ResizeInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Commands/CommandAPI.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

namespace Scenario
{

class DropProcessInIntervalHelper
{
public:
  DropProcessInIntervalHelper(
      const Scenario::IntervalModel& interval,
      std::optional<TimeVal> maxdur)
      : m_context{score::IDocument::documentContext(interval)}
      , m_macro{new Command::AddProcessInNewBoxMacro, m_context}
      , m_itv{interval}
  {
    // If the interval has no processes and nothing after, we will resize it
    if (interval.processes.empty() && maxdur)
    {
      if (auto resizer
          = m_context.app.interfaces<Scenario::IntervalResizerList>().make(
              m_itv, *maxdur))
        m_macro.submit(resizer);
    }
  }

  template <typename F>
  Process::ProcessModel* addProcess(F&& fun)
  {
    return fun(m_macro, m_itv);
  }

  void commit()
  {
    m_macro.showRack(m_itv);
    m_macro.commit();
  }

  Scenario::Command::Macro& macro() { return m_macro; }

private:
  const score::DocumentContext& m_context;
  Scenario::Command::Macro m_macro;
  const Scenario::IntervalModel& m_itv;
};


bool DropProcessInInterval::drop(
    const score::DocumentContext& ctx,
    const IntervalModel& cst,
    QPointF pos,
    const QMimeData& mime)
{
  if (mime.hasFormat("score/object-item-model-index"))
  {
    auto dat = score::unmarshall<Path<Process::ProcessModel>>(
        mime.data("score/object-item-model-index"));
    auto proc = dat.try_find(ctx);
    if (proc->parent() == &cst)
    {
      CommandDispatcher<>{ctx.commandStack}.submit(
          new Scenario::Command::AddLayerInNewSlot{cst, proc->id()});
    }
    return true;
  }

  const auto& handlers = ctx.app.interfaces<Process::ProcessDropHandlerList>();

  if (auto res = handlers.getDrop(mime, ctx); !res.empty())
  {
    auto t = handlers.getMaxDuration(res);
    DropProcessInIntervalHelper dropper(cst, t);

    score::Dispatcher_T disp{dropper.macro()};
    for (const auto& proc : res)
    {
      auto p = dropper.addProcess(
          [&](Scenario::Command::Macro& m,
              const IntervalModel& itv) -> Process::ProcessModel* {
            return m.createProcessInNewSlot(itv, proc.creation, pos);
          });
      if (p && proc.setup)
      {
        proc.setup(*p, disp);
      }
    }

    dropper.commit();
    return true;
  }
  return false;
}
}
