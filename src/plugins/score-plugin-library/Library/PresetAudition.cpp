#include "PresetAudition.hpp"

#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/PresetTree.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Command.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

namespace Library
{

PresetAudition::PresetAudition(QObject* parent)
    : QObject{parent}
{
}

PresetAudition::~PresetAudition()
{
  commit();
}

bool PresetAudition::canAudition(
    const Process::ProcessModel& proc, const Process::Preset& p)
{
  return !proc.presetChangesPorts(p);
}

bool PresetAudition::preview(const Process::ProcessModel& proc, const Process::Preset& p)
{
  if(!canAudition(proc, p))
    return false;
  if(m_proc && m_proc != &proc)
    commit();

  const auto& ctx = score::IDocument::documentContext(proc);
  if(!m_cmd)
  {
    // A session starts: what it replaces, and the undo / redo that would
    // act on the state under it held back until it ends
    m_proc = &proc;
    m_doc = &ctx.document;
    m_cursorBefore = Process::presetCursor(proc);
    ctx.commandStack.disableActions();
    // Anything else that changes the history (a command, an undo, a jump in
    // the history) ends the session first, so that its step comes before
    m_pushed = connect(
        &ctx.document.commandStack(), &score::CommandStack::aboutToChange, this,
        [this] { commit(); });
    m_gone = connect(&proc, &QObject::destroyed, this, [this] {
      // Its document went with it: nothing to undo or to keep
      m_cmd.reset();
      end();
    });
  }
  else
  {
    // Each preset replaces the previous one from the state before the
    // session: the cables of a process are saved and restored that way too
    m_cmd->undo(ctx);
    m_cmd.reset();
  }

  auto& factories = ctx.app.interfaces<Process::LoadPresetCommandFactoryList>();
  m_cmd.reset(factories.make(&Process::LoadPresetCommandFactory::make, proc, p, ctx));
  if(!m_cmd)
  {
    // Back to before the session: its previous preset was undone already
    Process::setPresetCursor(proc, m_cursorBefore);
    end();
    return false;
  }
  m_cmd->redo(ctx);
  Process::rememberPreset(proc, p);
  return true;
}

void PresetAudition::commit()
{
  if(!m_cmd)
    return;
  // Disconnected first: this push is the session's own
  disconnect(m_pushed);
  if(m_doc && m_proc)
    m_doc->context().commandStack.push(m_cmd.release());
  m_cmd.reset();
  end();
}

void PresetAudition::rollback()
{
  if(m_cmd && m_proc && m_doc)
  {
    m_cmd->undo(m_doc->context());
    Process::setPresetCursor(*m_proc, m_cursorBefore);
  }
  m_cmd.reset();
  end();
}

void PresetAudition::end()
{
  disconnect(m_pushed);
  disconnect(m_gone);
  if(m_doc)
    m_doc->context().commandStack.enableActions();
  m_proc = nullptr;
  m_doc = nullptr;
  m_cursorBefore.clear();
}
}
