#pragma once
#include <Process/Script/ScriptEditor.hpp>
#include <JS/JSProcessModel.hpp>
#include <JS/Commands/JSCommandFactory.hpp>


PROPERTY_COMMAND_T(JS, EditScript, ProcessModel::p_script, "Edit script")
SCORE_COMMAND_DECL_T(JS::EditScript)
