#pragma once
#include <Scenario/Commands/ScriptEditCommand.hpp>

#include <JS/Commands/JSCommandFactory.hpp>
#include <JS/JSProcessModel.hpp>
#include <score/command/AggregateCommand.hpp>
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

// State commands
namespace JS
{
class UpdateStateElement : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), UpdateStateElement, "Update a JS state element")
public:
  using score::Command::Command;
  UpdateStateElement(const JS::ProcessModel& obj, const QString& k, const ossia::value& v)
      : m_path{obj}
      , m_k{k}
      , m_new{v}
  {
    const auto& cur = obj.state();
    if(auto it = cur.find(k); it != cur.end()) {
      m_old = it->second;
    }
  }

  ~UpdateStateElement() override { }

  bool compatible(const JS::ProcessModel& obj, const QString& k, const ossia::value& v) const noexcept
  {
    return m_k == k;
  }

  void update(const JS::ProcessModel& obj, const QString& k, const ossia::value& v)
  {
    m_new = v;
  }

  void undo(const score::DocumentContext& ctx) const final override
  {
    m_path.find(ctx).updateState(m_k, m_old);
  }

  void redo(const score::DocumentContext& ctx) const final override
  {
    m_path.find(ctx).updateState(m_k, m_new);
  }

private:
  void serializeImpl(DataStreamInput& s) const final override
  {
    s << m_path << m_k << m_old << m_new;
  }

  void deserializeImpl(DataStreamOutput& s) final override
  {
    s >> m_path >> m_k >> m_old >> m_new;
  }

  Path<JS::ProcessModel> m_path;
  QString m_k;
  ossia::value m_old;
  ossia::value m_new;
};

class UpdateStateMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), UpdateStateMacro, "Update JS state")
};
}

PROPERTY_COMMAND_T(JS, ReplaceState, ProcessModel::p_state, "Replace JS state")
SCORE_COMMAND_DECL_T(JS::ReplaceState)
