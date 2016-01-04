#pragma once
#include <Autom3D/Commands/Autom3DCommandFactory.hpp>
#include <iscore/command/PropertyCommand.hpp>

namespace Autom3D
{
class ProcessModel;
class SetMin final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(CommandFactoryName(), SetMin, "Set curve minimum")
    public:

        SetMin(Path<ProcessModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "min", newval}
        {

        }
};
}
