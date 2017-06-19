#pragma once
#include <QByteArray>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace Process
{
class ProcessModel;
}

namespace Scenario
{
class ConstraintModel;
namespace Command
{
/**
        * @brief The AddLayerInNewSlot class
        */
class ISCORE_PLUGIN_SCENARIO_EXPORT AddLayerInNewSlot final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), AddLayerInNewSlot, "Add a new layer")
public:
  AddLayerInNewSlot(
      Path<ConstraintModel>&& constraintPath,
      Id<Process::ProcessModel> process);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

  Id<Process::ProcessModel> processId() const
  {
    return m_processId;
  }

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ConstraintModel> m_path;
  Id<Process::ProcessModel> m_processId{};
};
}
}
