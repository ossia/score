#pragma once
#include <Scenario/Commands/ScriptEditCommand.hpp>

#include <JS/Commands/JSCommandFactory.hpp>
#include <JS/JSProcessModel.hpp>
namespace JS
{
class EditScript
    : public Scenario::EditScript<JS::ProcessModel, JS::ProcessModel::p_script>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), EditScript, "Edit a JS script")
public:
  using Scenario::EditScript<JS::ProcessModel, JS::ProcessModel::p_script>::EditScript;
};
}

namespace score
{
template <>
struct StaticPropertyCommand<JS::ProcessModel::p_script> : JS::EditScript
{
  using EditScript::EditScript;
};
}
