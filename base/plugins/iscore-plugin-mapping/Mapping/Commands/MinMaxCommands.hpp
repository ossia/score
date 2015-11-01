#pragma once
#include <Mapping/Commands/MappingCommandFactory.hpp>
#include <iscore/command/PropertyCommand.hpp>

class MappingModel;
class SetMappingSourceMin : public iscore::PropertyCommand
{
        ISCORE_PROPERTY_COMMAND_DECL(MappingCommandFactoryName(), SetMappingSourceMin, "Set mapping source minimum")
    public:
        SetMappingSourceMin(Path<MappingModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "sourceMin", newval, factoryName(), commandName(), description()}
        {

        }
};

class SetMappingSourceMax : public iscore::PropertyCommand
{
        ISCORE_PROPERTY_COMMAND_DECL(MappingCommandFactoryName(), SetMappingSourceMax, "Set mapping source Maximum")
    public:
        SetMappingSourceMax(Path<MappingModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "sourceMax", newval, factoryName(), commandName(), description()}
        {

        }
};

class SetMappingTargetMin : public iscore::PropertyCommand
{
        ISCORE_PROPERTY_COMMAND_DECL(MappingCommandFactoryName(), SetMappingTargetMin, "Set mapping Target minimum")
    public:
        SetMappingTargetMin(Path<MappingModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "targetMin", newval, factoryName(), commandName(), description()}
        {

        }
};

class SetMappingTargetMax : public iscore::PropertyCommand
{
        ISCORE_PROPERTY_COMMAND_DECL(MappingCommandFactoryName(), SetMappingTargetMax, "Set mapping Target Maximum")
    public:
        SetMappingTargetMax(Path<MappingModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "targetMax", newval, factoryName(), commandName(), description()}
        {

        }
};
