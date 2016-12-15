#include "AddNote.hpp"
#include <Midi/MidiProcess.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>

namespace Midi
{

AddNote::AddNote(const ProcessModel& model, const NoteData& n)
    : m_model{model}, m_id{getStrongId(model.notes)}, m_note{n}
{
}

void AddNote::undo() const
{
  m_model.find().notes.remove(m_id);
}

void AddNote::redo() const
{
  auto& model = m_model.find();
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
