#include "ProcessCreation.hpp"

#include <Process/Commands/EditPort.hpp>
#include <Process/Commands/Properties.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

#include <Library/ProcessesItemModel.hpp>

#include <Dataflow/Commands/EditConnection.hpp>

#include <score/command/Dispatchers/RuntimeDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/IdentifierGeneration.hpp>

namespace Scenario
{
// Signal ports first: a control port of the right type is still a valid target
// (a value chain feeding a parameter), just never the preferred one.
const Process::Inlet*
firstInletOfType(const Process::ProcessModel& proc, Process::PortType type) noexcept
{
  for(auto* inlet : proc.inlets())
    if(inlet->type() == type && !qobject_cast<Process::ControlInlet*>(inlet))
      return inlet;
  for(auto* inlet : proc.inlets())
    if(inlet->type() == type)
      return inlet;
  return nullptr;
}

const Process::Outlet*
firstOutletOfType(const Process::ProcessModel& proc, Process::PortType type) noexcept
{
  for(auto* outlet : proc.outlets())
    if(outlet->type() == type && !qobject_cast<Process::ControlOutlet*>(outlet))
      return outlet;
  for(auto* outlet : proc.outlets())
    if(outlet->type() == type)
      return outlet;
  return nullptr;
}

const Process::Outlet* firstSignalOutlet(const Process::ProcessModel& proc) noexcept
{
  for(auto* outlet : proc.outlets())
    if(!qobject_cast<Process::ControlOutlet*>(outlet))
      return outlet;
  return nullptr;
}

//! Which outlet of `parentProcess` the newly created `proc` gets chained after:
//! `preferred` when the new process accepts its type, else the first other
//! signal outlet whose type it does accept.
static std::pair<const Process::Outlet*, const Process::Inlet*> matchOutletToNewProcess(
    const Process::ProcessModel& parentProcess, const Process::Outlet& preferred,
    const Process::ProcessModel& proc, bool tryOtherOutlets) noexcept
{
  if(auto in = firstInletOfType(proc, preferred.type()))
    return {&preferred, in};

  if(tryOtherOutlets)
  {
    for(auto* out : parentProcess.outlets())
    {
      if(out == &preferred || qobject_cast<Process::ControlOutlet*>(out))
        continue;
      if(auto in = firstInletOfType(proc, out->type()))
        return {out, in};
    }
  }
  return {&preferred, nullptr};
}

bool canInsertProcessInCable(
    const Process::Context& ctx, const Process::ProcessModel& proc,
    const Process::Cable& cbl)
{
  auto source = cbl.source().try_find(ctx);
  auto sink = cbl.sink().try_find(ctx);
  if(!source || !sink)
    return false;
  if(source->parent() == &proc || sink->parent() == &proc)
    return false;

  const auto type = source->type();
  return firstInletOfType(proc, type) || firstOutletOfType(proc, type);
}

void insertProcessInCable(
    score::Dispatcher& disp, const Process::Context& ctx,
    const Scenario::ScenarioDocumentModel& model, const Process::ProcessModel& proc,
    const Process::Cable& cbl)
{
  auto source = cbl.source().try_find(ctx);
  auto sink = cbl.sink().try_find(ctx);
  if(!source || !sink)
    return;
  if(source->parent() == &proc || sink->parent() == &proc)
    return;

  const auto type = source->type();
  const auto cable_type = cbl.type();
  auto new_inlet = firstInletOfType(proc, type);
  auto new_outlet = firstOutletOfType(proc, type);

  auto connect = [&](const Process::Port& from, const Process::Port& to,
                     Process::CableType t) {
    auto [src, snk] = Dataflow::getPortsForConnection(from, to);
    if(!src || !snk)
      return;
    disp.submit(
        new Dataflow::CreateCable{model, getStrongId(model.cables), t, *src, *snk});
  };

  if(new_inlet && new_outlet)
  {
    // The cable and everything reachable from it are gone once this runs.
    disp.submit(new Dataflow::RemoveCable{model, cbl});
    connect(*source, *new_inlet, Process::CableType::ImmediateGlutton);
    connect(*new_outlet, *sink, cable_type);
  }
  else if(new_inlet)
  {
    connect(*source, *new_inlet, Process::CableType::ImmediateGlutton);
  }
  else if(new_outlet)
  {
    connect(*new_outlet, *sink, Process::CableType::ImmediateGlutton);
  }
}

// The port a cable starts from may belong to a process nested deeper than
// `itv`: only its direct children share the interval's nodal coordinates.
static QPointF
positionAfterSourceOf(const IntervalModel& itv, const Process::Port& source) noexcept
{
  if(auto proc = qobject_cast<Process::ProcessModel*>(source.parent()))
    if(proc->parent() == &itv)
      return newProcessPositionAfter(itv, *proc);
  return newProcessPosition(itv);
}

void createProcessInCable(
    const Process::Context& context, const Scenario::ScenarioDocumentModel& model,
    const Process::ProcessData& dat, std::optional<TimeVal> tv,
    std::function<void(Process::ProcessModel&, score::Dispatcher&)> setup,
    const Process::Cable& cbl)
{
  auto& orig_source = cbl.source().find(context);
  SCORE_ASSERT(orig_source.type() == cbl.sink().find(context).type());

  if(auto parent_itv = Scenario::closestParentInterval(&orig_source))
  {
    Command::Macro m{new Command::DropProcessInIntervalMacro, context};

    const auto pos = positionAfterSourceOf(*parent_itv, orig_source);

    auto proc = m.createProcessInNewSlot(*parent_itv, dat, pos);
    if(proc)
    {
      score::Dispatcher_T<Scenario::Command::Macro> disp{m};
      if(setup)
        setup(*proc, disp);

      // TODO all of this should be made atomic...
      insertProcessInCable(disp, context, model, *proc, cbl);

      context.selectionStack.pushNewSelection({proc});
    }

    m.commit();
  }
}

void loadPresetInCable(
    const Process::Context& context, const Scenario::ScenarioDocumentModel& model,
    const Process::Preset& dat, const Process::Cable& cbl)
{
  auto& orig_source = cbl.source().find(context);
  SCORE_ASSERT(orig_source.type() == cbl.sink().find(context).type());

  if(auto parent_itv = Scenario::closestParentInterval(&orig_source))
  {
    Command::Macro m{new Command::DropProcessInIntervalMacro, context};

    const auto pos = positionAfterSourceOf(*parent_itv, orig_source);

    auto proc = m.loadProcessFromPreset(*parent_itv, dat, pos);
    if(proc)
    {
      score::Dispatcher_T<Scenario::Command::Macro> disp{m};

      // TODO all of this should be made atomic...
      insertProcessInCable(disp, context, model, *proc, cbl);

      context.selectionStack.pushNewSelection({proc});
    }

    m.commit();
  }
}

void createProcessBeforePort(
    Scenario::ScenarioDocumentPresenter& parent, const Process::ProcessData& dat,
    std::optional<TimeVal> tv,
    std::function<void(Process::ProcessModel&, score::Dispatcher&)> setup,
    const Process::ProcessModel& parentProcess, const Process::Inlet& p)
{
  if(auto parent_itv = Scenario::closestParentInterval(const_cast<Process::Inlet*>(&p)))
  {
    Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};

    auto proc = m.createProcessInNewSlot(*parent_itv, dat, QPointF{});
    if(proc)
    {
      if(setup)
      {
        score::Dispatcher_T<Scenario::Command::Macro> disp{m};
        setup(*proc, disp);
      }

      const auto pos = newProcessPositionBefore(*parent_itv, parentProcess, proc);
      m.setProperty<Process::ProcessModel::p_position>(*proc, pos);

      // TODO all of this should be made atomic...
      if(auto new_outlet = firstOutletOfType(*proc, p.type()))
      {
        m.createCable(
            parent.model(), *new_outlet, p, Process::CableType::ImmediateGlutton);
      }

      // Move the address in the selected input to the matching inlet of the new process
      if(auto new_inlet = firstInletOfType(*proc, p.type()))
      {
        if(auto addr = p.address(); addr != State::AddressAccessor{})
        {
          m.setProperty<Process::Port::p_address>(*new_inlet, addr);
          m.setProperty<Process::Port::p_address>(p, State::AddressAccessor{});
        }
      }

      parent.context().selectionStack.pushNewSelection({proc});
    }
    m.commit();
  }
}

void createProcessAfterPort(
    Scenario::ScenarioDocumentPresenter& parent, const Process::ProcessData& dat,
    std::optional<TimeVal> tv,
    std::function<void(Process::ProcessModel&, score::Dispatcher&)> setup,
    const Process::ProcessModel& parentProcess, const Process::Outlet& p,
    bool tryOtherOutlets)
{
  if(auto parent_itv = Scenario::closestParentInterval(const_cast<Process::Outlet*>(&p)))
  {
    Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};

    const auto pos = newProcessPositionAfter(*parent_itv, parentProcess);
    auto proc = m.createProcessInNewSlot(*parent_itv, dat, pos);
    if(proc)
    {
      if(setup)
      {
        score::Dispatcher_T<Scenario::Command::Macro> disp{m};
        setup(*proc, disp);
      }

      // TODO all of this should be made atomic...
      auto [src, new_inlet]
          = matchOutletToNewProcess(parentProcess, p, *proc, tryOtherOutlets);
      if(new_inlet)
      {
        m.createCable(
            parent.model(), *src, *new_inlet, Process::CableType::ImmediateGlutton);
      }

      // Move the address in the selected output to the matching outlet of the new process
      if(auto new_outlet = firstOutletOfType(*proc, src->type()))
      {
        if(auto addr = src->address(); addr != State::AddressAccessor{})
        {
          m.setProperty<Process::Port::p_address>(*new_outlet, addr);
          m.setProperty<Process::Port::p_address>(*src, State::AddressAccessor{});
        }
      }

      parent.context().selectionStack.pushNewSelection({proc});
    }
    m.commit();
  }
}

void loadPresetBeforePort(
    Scenario::ScenarioDocumentPresenter& parent, const Process::Preset& dat,
    const Process::ProcessModel& parentProcess, const Process::Inlet& p)
{
  if(auto parent_itv = Scenario::closestParentInterval(const_cast<Process::Inlet*>(&p)))
  {
    Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};

    auto proc = m.loadProcessFromPreset(*parent_itv, dat, QPointF{});
    if(proc)
    {
      const auto pos = newProcessPositionBefore(*parent_itv, parentProcess, proc);
      m.setProperty<Process::ProcessModel::p_position>(*proc, pos);

      // TODO all of this should be made atomic...
      if(auto new_outlet = firstOutletOfType(*proc, p.type()))
      {
        m.createCable(
            parent.model(), *new_outlet, p, Process::CableType::ImmediateGlutton);
      }

      // Move the address in the selected input to the matching inlet of the new process
      if(auto new_inlet = firstInletOfType(*proc, p.type()))
      {
        if(auto addr = p.address(); addr != State::AddressAccessor{})
        {
          m.setProperty<Process::Port::p_address>(*new_inlet, addr);
          m.setProperty<Process::Port::p_address>(p, State::AddressAccessor{});
        }
      }

      parent.context().selectionStack.pushNewSelection({proc});
    }
    m.commit();
  }
}

void loadPresetAfterPort(
    Scenario::ScenarioDocumentPresenter& parent, const Process::Preset& dat,
    const Process::ProcessModel& parentProcess, const Process::Outlet& p,
    bool tryOtherOutlets)
{
  if(auto parent_itv = Scenario::closestParentInterval(const_cast<Process::Outlet*>(&p)))
  {
    Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};

    const auto pos = newProcessPositionAfter(*parent_itv, parentProcess);
    auto proc = m.loadProcessFromPreset(*parent_itv, dat, pos);
    if(proc)
    {
      // TODO all of this should be made atomic...
      auto [src, new_inlet]
          = matchOutletToNewProcess(parentProcess, p, *proc, tryOtherOutlets);
      if(new_inlet)
      {
        m.createCable(
            parent.model(), *src, *new_inlet, Process::CableType::ImmediateGlutton);
      }

      // Move the address in the selected output to the matching outlet of the new process
      if(auto new_outlet = firstOutletOfType(*proc, src->type()))
      {
        if(auto addr = src->address(); addr != State::AddressAccessor{})
        {
          m.setProperty<Process::Port::p_address>(*new_outlet, addr);
          m.setProperty<Process::Port::p_address>(*src, State::AddressAccessor{});
        }
      }

      parent.context().selectionStack.pushNewSelection({proc});
    }
    m.commit();
  }
}
}
