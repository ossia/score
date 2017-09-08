#pragma once
#include <JS/Commands/JSCommandFactory.hpp>
#include <QString>
#include <score/command/Command.hpp>

#include <score/model/path/Path.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace JS
{
class ProcessModel;
class StateProcess;

class EditScript final : public score::Command
{
  SCORE_COMMAND_DECL(JS::CommandFactoryName(), EditScript, "Edit a JS script")
public:
  EditScript(const ProcessModel& model, const QString& text);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_model;
  QString m_old, m_new;
};

class EditStateScript final : public score::Command
{
  SCORE_COMMAND_DECL(
      JS::CommandFactoryName(), EditStateScript, "Edit a JS state script")
public:
  EditStateScript(const StateProcess& model, const QString& text);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<StateProcess> m_model;
  QString m_old, m_new;
};
}
