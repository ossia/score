#pragma once
#include <JS/Commands/JSCommandFactory.hpp>

#include <score/command/Command.hpp>

#include <QByteArray>
#include <QString>

namespace JS
{
/**
 * @brief An undoable edit performed by a script.
 *
 * A script can change state the engine knows nothing about - the QML side of
 * an application, an external socket - and until now that state could not take
 * part in undo: there was no command for it, so an edit mixing it with engine
 * edits half-undid.
 *
 * The command carries a handler name and the payload for each direction,
 * rather than the two closures the interface suggests: a command is
 * serialized, both for the crash-recovery backup and for the network plug-in,
 * and a QJSValue cannot be. Handlers are registered by name with
 * Score.registerCommandHandler().
 *
 * A handler that is not registered when the command runs - a backup restored
 * before the script that owns it has loaded - is reported once and skipped.
 */
class ScriptCommand final : public score::Command
{
  SCORE_COMMAND_DECL(JS::CommandFactoryName(), ScriptCommand, "Script edit")
public:
  ScriptCommand(QString handler, QByteArray undoPayload, QByteArray redoPayload);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

private:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

  void dispatch(const QByteArray& payload) const;

  QString m_handler;
  QByteArray m_undo;
  QByteArray m_redo;
};
}
