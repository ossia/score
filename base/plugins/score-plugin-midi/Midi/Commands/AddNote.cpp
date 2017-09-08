// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddNote.hpp"
#include <Midi/MidiProcess.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/IdentifierGeneration.hpp>

namespace Midi
{

AddNote::AddNote(const ProcessModel& model, const NoteData& n)
    : m_model{model}, m_id{getStrongId(model.notes)}, m_note{n}
{
}

void AddNote::undo(const score::DocumentContext& ctx) const
{
  m_model.find(ctx).notes.remove(m_id);
}

void AddNote::redo(const score::DocumentContext& ctx) const
{
  auto& model = m_model.find(ctx);
  model.notes.add(new Note{m_id, m_note, &model});
}

void AddNote::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_id << m_note;
}

void AddNote::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_id >> m_note;
}
}
