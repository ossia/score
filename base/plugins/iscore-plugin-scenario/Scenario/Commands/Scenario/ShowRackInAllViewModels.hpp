#pragma once
#include <QMap>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class RackModel;
class ConstraintModel;
class ConstraintViewModel;

namespace Command
{

class ISCORE_PLUGIN_SCENARIO_EXPORT ShowRackInAllViewModels final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      ShowRackInAllViewModels,
      "Show a rack everywhere")
public:
  ShowRackInAllViewModels(
      Path<ConstraintModel>&& constraint_path, Id<RackModel> rackId);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ConstraintModel> m_constraintPath;
  Id<RackModel> m_rackId{};

  QMap<Id<ConstraintViewModel>, OptionalId<RackModel>> m_previousRacks;
};
}
}
