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
/**
 * @brief The HideRackInViewModel class
 *
 * For a given constraint view model, hides the rack.
 * Can only be called if a rack was being displayed.
 */
class HideRack final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), HideRack, "Hide rack")
public:
  HideRack(const Scenario::ConstraintModel& constraint);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::ConstraintModel> m_path;
};
}
}
