#pragma once
#include <State/Address.hpp>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>

#include <score_plugin_scenario_export.h>

namespace score
{
struct DocumentContext;
}
namespace Scenario::Command
{
class RebindReferenceMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), RebindReferenceMacro, "Rebind reference")
};

//! Replaces `from` with `to` in the referrer through commands, so that `to` gets anchored
SCORE_PLUGIN_SCENARIO_EXPORT void rebindReference(
    QObject& referrer, const ::State::Address& from, const ::State::Address& to,
    const score::DocumentContext& ctx);
}
