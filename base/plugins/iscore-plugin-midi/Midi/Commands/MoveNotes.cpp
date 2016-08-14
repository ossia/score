#include "MoveNotes.hpp"
#include <Midi/MidiProcess.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

namespace Midi
{

MoveNotes::MoveNotes(
        const ProcessModel& model,
        const std::vector<Id<Note>>& to_move,
        double delta):
  m_model{model},
  m_toMove{to_move},
  m_delta{delta}
{
}

void MoveNotes::undo() const
{
    auto& model = m_model.find();
    for(auto& note : m_toMove)
    {
        auto& n = model.notes.at(note);
        n.setStart(n.start() - m_delta);
    }
}

void MoveNotes::redo() const
{
    auto& model = m_model.find();
    for(auto& note : m_toMove)
    {
        auto& n = model.notes.at(note);
        n.setStart(n.start() + m_delta);
    }
}

void MoveNotes::serializeImpl(DataStreamInput& s) const
{
    s << m_model << m_toMove << m_delta;
}

void MoveNotes::deserializeImpl(DataStreamOutput& s)
{
    s >> m_model >> m_toMove >> m_delta;
}


}
