#include "ScriptCommand.hpp"

#include <JS/ApplicationPlugin.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace JS
{

ScriptCommand::ScriptCommand(
    QString handler, QByteArray undoPayload, QByteArray redoPayload)
    : m_handler{std::move(handler)}
    , m_undo{std::move(undoPayload)}
    , m_redo{std::move(redoPayload)}
{
}

void ScriptCommand::undo(const score::DocumentContext&) const
{
  dispatch(m_undo);
}

void ScriptCommand::redo(const score::DocumentContext&) const
{
  dispatch(m_redo);
}

void ScriptCommand::dispatch(const QByteArray& payload) const
{
  auto plug = score::GUIAppContext().findGuiApplicationPlugin<JS::ApplicationPlugin>();
  if(!plug)
    return;
  plug->callCommandHandler(m_handler, payload);
}

void ScriptCommand::serializeImpl(DataStreamInput& s) const
{
  s << m_handler << m_undo << m_redo;
}

void ScriptCommand::deserializeImpl(DataStreamOutput& s)
{
  s >> m_handler >> m_undo >> m_redo;
}
}
