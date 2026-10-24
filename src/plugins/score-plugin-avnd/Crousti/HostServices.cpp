#include "HostServices.hpp"

#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/Process.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <QDebug>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

#include <chrono>

namespace oscr
{
namespace
{
//! The interval that holds `proc` directly in a scenario, and the date of
//! `proc`'s own start in that scenario. A process inside a box of a loop or a
//! nodal view is placed from the start of the outermost box in the scenario.
struct placement
{
  const Scenario::ProcessModel* scenario{};
  const Scenario::IntervalModel* interval{};
  TimeVal offset{};
};

placement findScenario(const Process::ProcessModel& proc)
{
  placement p;
  TimeVal offset{};
  bool inner = false;
  for(QObject* o = proc.parent(); o; o = o->parent())
  {
    if(auto itv = qobject_cast<const Scenario::IntervalModel*>(o))
    {
      if(auto sc = qobject_cast<const Scenario::ProcessModel*>(itv->parent()))
      {
        p.scenario = sc;
        p.interval = itv;
        p.offset = inner ? itv->date() : itv->date() + offset;
        return p;
      }
      inner = true;
    }
  }
  return p;
}
}

void sendToTimeline(
    const Process::ProcessModel& proc, const score::DocumentContext& ctx,
    const halp::timeline_clip& clip)
{
  if(clip.file.empty())
    return;

  const QString file = QString::fromStdString(clip.file);
  if(!QFileInfo::exists(file))
  {
    qWarning() << "Send to timeline:" << file << "does not exist";
    return;
  }

  // Several views of one process each ask: only the first one is heeded.
  {
    using clock = std::chrono::steady_clock;
    static const Process::ProcessModel* last_proc{};
    static std::string last_file;
    static clock::time_point last_time{};
    const auto now = clock::now();
    if(last_proc == &proc && last_file == clip.file && now - last_time < std::chrono::milliseconds(500))
      return;
    last_proc = &proc;
    last_file = clip.file;
    last_time = now;
  }

  const auto [scenario, interval, offset] = findScenario(proc);
  if(!scenario || !interval)
  {
    qWarning() << "Send to timeline:" << proc.metadata().getName()
               << "is not in a scenario; drag the take instead";
    return;
  }

  QMimeData mime;
  mime.setUrls({QUrl::fromLocalFile(file)});
  const auto& handlers = ctx.app.interfaces<Process::ProcessDropHandlerList>();
  auto drops = handlers.getDrop(mime, ctx);
  if(drops.empty())
  {
    qWarning() << "Send to timeline: no process plays" << file;
    return;
  }

  const TimeVal start = offset + TimeVal::fromMsecs(clip.date * 1000.);
  const TimeVal duration
      = handlers.getMaxDuration(drops).value_or(TimeVal::fromMsecs(10000.));
  const double y = std::clamp(
      interval->heightPercentage() + 0.08 + 0.05 * clip.lane, 0.02, 0.95);

  // The box hangs off the scenario's start, through an empty interval up to
  // the take's date: a box nothing leads to does not play.
  Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, ctx};
  auto& from = m.createState(*scenario, scenario->startEvent().id(), y);
  Id<Scenario::StateModel> box_start = from.id();
  if(start > TimeVal::zero())
    box_start = m.createIntervalAfter(*scenario, from.id(), {start, y}).endState();
  auto& box = m.createIntervalAfter(*scenario, box_start, {start + duration, y});
  score::Dispatcher_T disp{m};
  for(const auto& drop : drops)
  {
    if(auto p = m.createProcessInNewSlot(box, drop.creation); p && drop.setup)
      drop.setup(*p, disp);
  }
  m.submit(new Scenario::Command::ChangeElementName{box, QFileInfo{file}.completeBaseName()});
  m.showRack(box);
  m.commit();
}
}
