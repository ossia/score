#include "ProcessCreation.hpp"

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Process/Commands/EditPort.hpp>
#include <Process/Commands/Properties.hpp>

#include <Library/ProcessesItemModel.hpp>

#include <score/selection/SelectionStack.hpp>

namespace Scenario
{
void createProcessInCable(
    Scenario::ScenarioDocumentPresenter& parent,
    const Library::ProcessData& dat,
    const Process::Cable& cbl)
{
  auto& orig_source = cbl.source().find(parent.context());
  auto& orig_sink = cbl.sink().find(parent.context());
  SCORE_ASSERT(orig_source.type() == orig_sink.type());
  auto type = orig_source.type();

  if(auto parent_itv = Scenario::closestParentInterval(&orig_source))
  {
    Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};

    auto pos = QPointF{};
    if(auto parent_source_proc = qobject_cast<Process::ProcessModel*>(orig_source.parent()))
    {
      pos = parent_source_proc->position();
      pos.rx() += parent_source_proc->size().width() + 40;
    }

    auto proc = m.createProcessInNewSlot(*parent_itv, dat, pos);
    if(proc)
    {
      // TODO all of this should be made atomic...
      if(!proc->inlets().empty() && !proc->outlets().empty())
      {
        auto new_inlet = proc->inlets()[0];
        auto new_outlet = proc->outlets()[0];
        // Create a cable from the output to the input
        if(new_inlet->type() == type && new_outlet->type() == type)
        {
          // orig_source goes into new_inlet
          // new_outlet goes into orig_sink
          m.removeCable(parent.model(), cbl);
          m.createCable(parent.model(), orig_source, *new_inlet);
          m.createCable(parent.model(), *new_outlet, orig_sink);
        }

        parent.context().selectionStack.pushNewSelection({proc});
      }
    }

    m.commit();
  }
}

void loadPresetInCable(
    Scenario::ScenarioDocumentPresenter& parent,
    const Process::Preset& dat,
    const Process::Cable& cbl)
{
  auto& orig_source = cbl.source().find(parent.context());
  auto& orig_sink = cbl.sink().find(parent.context());
  SCORE_ASSERT(orig_source.type() == orig_sink.type());
  auto type = orig_source.type();

  if(auto parent_itv = Scenario::closestParentInterval(&orig_source))
  {
    Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};

    auto pos = QPointF{};
    if(auto parent_source_proc = qobject_cast<Process::ProcessModel*>(orig_source.parent()))
    {
      pos = parent_source_proc->position();
      pos.rx() += parent_source_proc->size().width() + 40;
    }

    auto proc = m.loadProcessFromPreset(*parent_itv, dat, pos);
    if(proc)
    {
      // TODO all of this should be made atomic...
      if(!proc->inlets().empty() && !proc->outlets().empty())
      {
        auto new_inlet = proc->inlets()[0];
        auto new_outlet = proc->outlets()[0];
        // Create a cable from the output to the input
        if(new_inlet->type() == type && new_outlet->type() == type)
        {
          // orig_source goes into new_inlet
          // new_outlet goes into orig_sink
          m.removeCable(parent.model(), cbl);
          m.createCable(parent.model(), orig_source, *new_inlet);
          m.createCable(parent.model(), *new_outlet, orig_sink);
        }

        parent.context().selectionStack.pushNewSelection({proc});
      }
    }

    m.commit();
  }
}

void createProcessBeforePort(
    Scenario::ScenarioDocumentPresenter& parent,
    const Library::ProcessData& dat,
    const Process::ProcessModel& parentProcess,
    const Process::Inlet& p)
{
  if(auto parent_itv = Scenario::closestParentInterval(const_cast<Process::Inlet*>(&p)))
  {
    Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};

    auto proc = m.createProcessInNewSlot(*parent_itv, dat, QPointF{});
    if(proc)
    {
      auto pos = parentProcess.position();
      pos.rx() -= proc->size().width() + 40;
      m.setProperty<Process::ProcessModel::p_position>(*proc, pos);

      // TODO all of this should be made atomic...
      if(!proc->outlets().empty())
      {
        auto new_outlet = proc->outlets()[0];
        // Create a cable from the output to the input
        if(new_outlet->type() == p.type())
        {
          m.createCable(parent.model(), *new_outlet, p);
        }

        if(!proc->inlets().empty())
        {
          auto new_inlet = proc->inlets()[0];
          // Move the address in the selected output to the first outlet of the new process
          if(new_inlet->type() == p.type())
          {
            if(auto addr = p.address(); addr != State::AddressAccessor{})
            {
              m.setProperty<Process::Port::p_address>(*new_inlet, addr);
              m.setProperty<Process::Port::p_address>(p, State::AddressAccessor{});
            }
          }
        }
        parent.context().selectionStack.pushNewSelection({proc});
      }
    }
    m.commit();
  }
}

void createProcessAfterPort(
    Scenario::ScenarioDocumentPresenter& parent,
    const Library::ProcessData& dat,
    const Process::ProcessModel& parentProcess,
    const Process::Outlet& p)
{
  if(auto parent_itv = Scenario::closestParentInterval(const_cast<Process::Outlet*>(&p)))
  {
    Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};

    auto pos = parentProcess.position();
    pos.rx() += parentProcess.size().width() + 40;
    auto proc = m.createProcessInNewSlot(*parent_itv, dat, pos);
    if(proc)
    {
      // TODO all of this should be made atomic...
      if(!proc->inlets().empty())
      {
        auto new_inlet = proc->inlets()[0];
        // Create a cable from the output to the input
        if(new_inlet->type() == p.type())
        {
          m.createCable(parent.model(), p, *new_inlet);
        }

        if(!proc->outlets().empty())
        {
          auto new_outlet = proc->outlets()[0];
          // Move the address in the selected output to the first outlet of the new process
          if(new_outlet->type() == p.type())
          {
            if(auto addr = p.address(); addr != State::AddressAccessor{})
            {
              m.setProperty<Process::Port::p_address>(*new_outlet, addr);
              m.setProperty<Process::Port::p_address>(p, State::AddressAccessor{});
            }
          }
        }
        parent.context().selectionStack.pushNewSelection({proc});
      }
    }
    m.commit();
  }
}


void loadPresetBeforePort(
    Scenario::ScenarioDocumentPresenter& parent,
    const Process::Preset& dat,
    const Process::ProcessModel& parentProcess,
    const Process::Inlet& p)
{
  if(auto parent_itv = Scenario::closestParentInterval(const_cast<Process::Inlet*>(&p)))
  {
    Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};

    auto proc = m.loadProcessFromPreset(*parent_itv, dat, QPointF{});
    if(proc)
    {
      auto pos = parentProcess.position();
      pos.rx() -= proc->size().width() + 40;
      m.setProperty<Process::ProcessModel::p_position>(*proc, pos);

      // TODO all of this should be made atomic...
      if(!proc->outlets().empty())
      {
        auto new_outlet = proc->outlets()[0];
        // Create a cable from the output to the input
        if(new_outlet->type() == p.type())
        {
          m.createCable(parent.model(), *new_outlet, p);
        }

        if(!proc->inlets().empty())
        {
          auto new_inlet = proc->inlets()[0];
          // Move the address in the selected output to the first outlet of the new process
          if(new_inlet->type() == p.type())
          {
            if(auto addr = p.address(); addr != State::AddressAccessor{})
            {
              m.setProperty<Process::Port::p_address>(*new_inlet, addr);
              m.setProperty<Process::Port::p_address>(p, State::AddressAccessor{});
            }
          }
        }
        parent.context().selectionStack.pushNewSelection({proc});
      }
    }
    m.commit();
  }
}

void loadPresetAfterPort(
    Scenario::ScenarioDocumentPresenter& parent,
    const Process::Preset& dat,
    const Process::ProcessModel& parentProcess,
    const Process::Outlet& p)
{
  if(auto parent_itv = Scenario::closestParentInterval(const_cast<Process::Outlet*>(&p)))
  {
    Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};

    auto pos = parentProcess.position();
    pos.rx() += parentProcess.size().width() + 40;
    auto proc = m.loadProcessFromPreset(*parent_itv, dat, pos);
    if(proc)
    {
      // TODO all of this should be made atomic...
      if(!proc->inlets().empty())
      {
        auto new_inlet = proc->inlets()[0];
        // Create a cable from the output to the input
        if(new_inlet->type() == p.type())
        {
          m.createCable(parent.model(), p, *new_inlet);
        }

        if(!proc->outlets().empty())
        {
          auto new_outlet = proc->outlets()[0];
          // Move the address in the selected output to the first outlet of the new process
          if(new_outlet->type() == p.type())
          {
            if(auto addr = p.address(); addr != State::AddressAccessor{})
            {
              m.setProperty<Process::Port::p_address>(*new_outlet, addr);
              m.setProperty<Process::Port::p_address>(p, State::AddressAccessor{});
            }
          }
        }
        parent.context().selectionStack.pushNewSelection({proc});
      }
    }
    m.commit();
  }
}
}
