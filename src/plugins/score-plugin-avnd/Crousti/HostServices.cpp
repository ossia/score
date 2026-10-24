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

#include <algorithm>
#include <chrono>
#include <vector>

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
    struct sent
    {
      const Process::ProcessModel* proc;
      std::string file;
      clock::time_point time;
    };
    static std::vector<sent> recent;
    const auto now = clock::now();
    std::erase_if(recent, [now](const sent& s) {
      return now - s.time > std::chrono::milliseconds(500);
    });
    for(const auto& s : recent)
      if(s.proc == &proc && s.file == clip.file)
        return;
    recent.push_back({&proc, clip.file, now});
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

  // A take that began before the scenario did, e.g. a capture of what was
  // heard before the looper's box started, is placed at the scenario's start:
  // whole, rather than cut short by the time it is early.
  const TimeVal start
      = std::max<TimeVal>(offset + TimeVal::fromMsecs(clip.date * 1000.), TimeVal::zero());
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
