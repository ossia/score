#pragma once
#include <Scenario/Commands/ScriptEditCommand.hpp>

#include <JS/Commands/JSCommandFactory.hpp>
#include <JS/JSProcessModel.hpp>
namespace JS
{
class EditJSScript
    : public Scenario::EditScript<JS::ProcessModel, JS::ProcessModel::p_script>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), EditJSScript, "Edit a script")
public:
  using EditScript::EditScript;
};
}

namespace score
{
template <>
struct StaticPropertyCommand<JS::ProcessModel::p_script> : JS::EditJSScript
{
  using EditJSScript::EditJSScript;
};
}
