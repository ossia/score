#include "RemoveNotes.hpp"
#include <Midi/MidiProcess.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

namespace Midi
{

RemoveNotes::RemoveNotes(
        const ProcessModel& model,
        const std::vector<Id<Note>>& notes):
  m_model{model}
{
    for(auto id : notes)
    {
        auto& note = model.notes.at(id);
        m_notes.push_back(qMakePair(id, NoteData{note.start(), note.duration(), note.pitch(), note.velocity()}));
    }
}

void RemoveNotes::undo() const
{
    auto& model = m_model.find();
    for(auto& note : m_notes)
    {
        model.notes.add(new Note{note.first, note.second, &model});
    }
}

void RemoveNotes::redo() const
{
    auto& model = m_model.find();
    for(auto& note : m_notes)
    {
        model.notes.remove(note.first);
    }
}

void RemoveNotes::serializeImpl(DataStreamInput& s) const
{
    s << m_model << m_notes;
}

void RemoveNotes::deserializeImpl(DataStreamOutput& s)
{
    s >> m_model >> m_notes;
}


}
