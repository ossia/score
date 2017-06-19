#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class ConstraintModel;
namespace Command
{
class SwapSlots final : public iscore::Command
{
  ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SwapSlots, "Swap slots")
public:
  SwapSlots(Path<ConstraintModel>&& rack, int pos1, int pos2);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ConstraintModel> m_path;
  int m_first{}, m_second{};
};
}
}
