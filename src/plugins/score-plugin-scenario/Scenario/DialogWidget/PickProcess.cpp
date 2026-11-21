#include "PickProcess.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>


#include <QCursor>

namespace Scenario
{
void pickProcess(
    const score::GUIApplicationContext& ctx, Process::ProcessFlags acceptable,
    std::function<void(const Library::PickedProcess&)> picked,
    Library::PickerOptions options)
{
  Library::ProcessPicker::open(
      ctx, acceptable, QCursor::pos(), std::move(picked), std::move(options));
}

const IntervalModel* addPickedProcessInNewBox(
    const score::DocumentContext& ctx, const Scenario::ProcessModel& scenario,
    const Scenario::Point& pt, const Library::PickedProcess& p)
{
  // As long as a process dropped in a scenario
  constexpr auto duration = TimeVal::fromMsecs(10000.);

  Command::Macro m{new Command::AddProcessInNewBoxMacro, ctx};
  auto& box = m.createBox(scenario, pt.date, pt.date + duration, pt.y);
  addStartOnPlayTrigger(m, startTimeSync(box, scenario));
  // Nothing is made if the process cannot be
  if(!addPickedProcess(m, box, p))
    return nullptr;
  m.showRack(box);
  if(const QString& name = p.preset ? p.preset->name : p.name; !name.isEmpty())
    m.submit(new Command::ChangeElementName{box, name});
  m.commit();
  return &box;
}

Library::PortFilter portFilter(const Process::Port& port)
{
  return {port.type(), qobject_cast<const Process::Outlet*>(&port) != nullptr};
}

Process::ProcessModel* addPickedProcess(
    Command::Macro& m, const IntervalModel& interval, const Library::PickedProcess& p,
    std::optional<QPointF> pos)
{
  const QPointF at = pos ? *pos : newProcessPosition(interval);
  if(p.preset)
    return m.loadProcessFromPreset(interval, *p.preset, at);
  return m.createProcessInNewSlot(interval, p.key, p.customData, at);
}
}
