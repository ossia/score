#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <iscore/command/PropertyCommand.hpp>

namespace Automation
{
class ProcessModel;
class SetMax final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(CommandFactoryName(), SetMax, "Set curve maximum")
        public:

        SetMax(Path<ProcessModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "max", newval}
        {

        }
};

// MOVEME
class SetTween final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(CommandFactoryName(), SetTween, "Set curve tween")
        public:

        SetTween(Path<ProcessModel>&& path, bool newval):
            iscore::PropertyCommand{std::move(path), "tween", newval}
        {

        }
};
}
