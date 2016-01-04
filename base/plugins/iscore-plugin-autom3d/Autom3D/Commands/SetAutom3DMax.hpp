#pragma once
#include <Autom3D/Commands/Autom3DCommandFactory.hpp>
#include <iscore/command/PropertyCommand.hpp>

namespace Autom3D
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
}
