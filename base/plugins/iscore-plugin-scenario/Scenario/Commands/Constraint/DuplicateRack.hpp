#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class RackModel;
namespace Command
{
class DuplicateRack final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), DuplicateRack, "Duplicate a rack")
public:
  DuplicateRack(const RackModel&);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<RackModel> m_rackPath;

  Id<RackModel> m_newRackId;
};
}
}
