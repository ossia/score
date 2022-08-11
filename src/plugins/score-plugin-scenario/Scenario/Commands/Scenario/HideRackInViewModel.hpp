#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class IntervalModel;
namespace Command
{
/**
 * @brief The HideRackInViewModel class
 *
 * For a given interval view model, hides the rack.
 * Can only be called if a rack was being displayed.
 */
class HideRack final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), HideRack, "Hide rack")
public:
  HideRack(const Scenario::IntervalModel& interval);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::IntervalModel> m_path;
};
}
}
