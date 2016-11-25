#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <iscore/command/PropertyCommand.hpp>

#include <State/Unit.hpp>

namespace Automation
{
class ProcessModel;
class SetMax final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(CommandFactoryName(), SetMax, "Set automation maximum")
        public:

        SetMax(Path<ProcessModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "max", newval}
        {

        }
};

// MOVEME
class SetTween final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(CommandFactoryName(), SetTween, "Set automation tween")
        public:

        SetTween(Path<ProcessModel>&& path, bool newval):
            iscore::PropertyCommand{std::move(path), "tween", newval}
        {

        }
};

class SetUnit final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(CommandFactoryName(), SetUnit, "Set automation unit")
        public:

        SetUnit(Path<ProcessModel>&& path, const State::Unit& newval):
            iscore::PropertyCommand{std::move(path), "unit", QVariant::fromValue(newval)}
        {

        }
};

}
