#pragma once
#include <Mapping/Commands/MappingCommandFactory.hpp>
#include <iscore/command/PropertyCommand.hpp>

namespace Mapping
{
class ProcessModel;
class SetMappingSourceMin final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(MappingCommandFactoryName(), SetMappingSourceMin, "Set mapping source minimum")
    public:
        SetMappingSourceMin(Path<ProcessModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "sourceMin", newval}
        {

        }
};

class SetMappingSourceMax final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(MappingCommandFactoryName(), SetMappingSourceMax, "Set mapping source Maximum")
    public:
        SetMappingSourceMax(Path<ProcessModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "sourceMax", newval}
        {

        }
};

class SetMappingTargetMin final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(MappingCommandFactoryName(), SetMappingTargetMin, "Set mapping Target minimum")
    public:
        SetMappingTargetMin(Path<ProcessModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "targetMin", newval}
        {

        }
};

class SetMappingTargetMax final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(MappingCommandFactoryName(), SetMappingTargetMax, "Set mapping Target Maximum")
    public:
        SetMappingTargetMax(Path<ProcessModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "targetMax", newval}
        {

        }
};
}
