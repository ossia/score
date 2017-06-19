#pragma once
#include <Mapping/Commands/MappingCommandFactory.hpp>
#include <iscore/command/PropertyCommand.hpp>

namespace Mapping
{
class ProcessModel;
class SetMappingSourceMin final : public iscore::PropertyCommand
{
  ISCORE_COMMAND_DECL(
      MappingCommandFactoryName(), SetMappingSourceMin,
      "Set mapping source minimum")
public:
  SetMappingSourceMin(const ProcessModel& path, double newval)
      : iscore::PropertyCommand{path, "sourceMin", newval}
  {
  }
};

class SetMappingSourceMax final : public iscore::PropertyCommand
{
  ISCORE_COMMAND_DECL(
      MappingCommandFactoryName(), SetMappingSourceMax,
      "Set mapping source Maximum")
public:
  SetMappingSourceMax(const ProcessModel& path, double newval)
      : iscore::PropertyCommand{path, "sourceMax", newval}
  {
  }
};

class SetMappingTargetMin final : public iscore::PropertyCommand
{
  ISCORE_COMMAND_DECL(
      MappingCommandFactoryName(), SetMappingTargetMin,
      "Set mapping Target minimum")
public:
  SetMappingTargetMin(const ProcessModel& path, double newval)
      : iscore::PropertyCommand{path, "targetMin", newval}
  {
  }
};

class SetMappingTargetMax final : public iscore::PropertyCommand
{
  ISCORE_COMMAND_DECL(
      MappingCommandFactoryName(), SetMappingTargetMax,
      "Set mapping Target Maximum")
public:
  SetMappingTargetMax(const ProcessModel& path, double newval)
      : iscore::PropertyCommand{path, "targetMax", newval}
  {
  }
};
}
