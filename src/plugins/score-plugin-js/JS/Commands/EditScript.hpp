#pragma once
#include <Scenario/Commands/ScriptEditCommand.hpp>

#include <JS/Commands/JSCommandFactory.hpp>
#include <JS/JSProcessModel.hpp>
namespace JS
{
class EditScript
    : public Scenario::EditScript<JS::ProcessModel, JS::ProcessModel::p_program>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), EditScript, "Edit a JS script")
public:
  using Scenario::EditScript<JS::ProcessModel, JS::ProcessModel::p_program>::EditScript;
};
}

namespace score
{
template <>
struct StaticPropertyCommand<JS::ProcessModel::p_program> : JS::EditScript
{
  using EditScript::EditScript;
};
}
