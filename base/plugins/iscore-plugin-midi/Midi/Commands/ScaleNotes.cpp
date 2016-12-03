#include "ScaleNotes.hpp"
#include <Midi/MidiProcess.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

namespace Midi
{
ScaleNotes::ScaleNotes(
    const ProcessModel& model,
    const std::vector<Id<Note>>& to_move,
    double delta)
    : m_model{model}, m_toScale{to_move}, m_delta{delta}
{
}

void ScaleNotes::undo() const
{
  auto& model = m_model.find();
  for (auto& note : m_toScale)
  {
    auto& n = model.notes.at(note);
    n.setDuration(n.duration() - m_delta);
  }
}

void ScaleNotes::redo() const
{
  auto& model = m_model.find();
  for (auto& note : m_toScale)
  {
    auto& n = model.notes.at(note);
    n.setDuration(n.duration() + m_delta);
  }
}

void ScaleNotes::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_toScale << m_delta;
}

void ScaleNotes::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_toScale >> m_delta;
}
}
