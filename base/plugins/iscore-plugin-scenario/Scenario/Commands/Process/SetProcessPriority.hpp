#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Process/Process.hpp>
#include <iscore/command/PropertyCommand.hpp>
namespace Scenario
{
namespace Command
{

class SetProcessPriority final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SetProcessPriority, "Set process priority")
    public:
        SetProcessPriority(
            Path<Process::ProcessModel>&& path,
            int32_t newval):
        iscore::PropertyCommand{std::move(path), "processPriority", QVariant::fromValue(newval)}
      {

      }
};

class SetProcessPriorityOverride final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SetProcessPriorityOverride, "Set process priority override")
    public:
        SetProcessPriorityOverride(
            Path<Process::ProcessModel>&& path,
            bool newval):
        iscore::PropertyCommand{std::move(path), "processPriorityOverride", QVariant::fromValue(newval)}
      {

      }
};
}

}
