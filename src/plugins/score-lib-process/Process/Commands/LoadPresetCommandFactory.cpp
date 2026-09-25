#include "LoadPresetCommandFactory.hpp"

#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Port.hpp>

#include <score/application/GUIApplicationContext.hpp>

namespace Process
{
LoadPresetCommandFactory::~LoadPresetCommandFactory() = default;
LoadPresetCommandFactoryList::~LoadPresetCommandFactoryList() = default;
ChangePortsCommand::~ChangePortsCommand() = default;
ChangePortsCommandFactory::~ChangePortsCommandFactory() = default;
ChangePortsCommandFactoryList::~ChangePortsCommandFactoryList() = default;

ChangePortsCommand* makeChangePortsCommand(
    const Process::ControlInlet& obj, ossia::value newval,
    const score::DocumentContext& ctx)
{
  if(!obj.changesPorts)
    return nullptr;
  return ctx.app.interfaces<ChangePortsCommandFactoryList>().make(
      &ChangePortsCommandFactory::make, obj, std::move(newval), ctx);
}

score::Command* makeSetControlCommand(
    const Process::ControlInlet& obj, ossia::value newval,
    const score::DocumentContext& ctx)
{
  if(auto cmd = makeChangePortsCommand(obj, newval, ctx))
    return cmd;
  return new SetValue{obj, std::move(newval)};
}
}
