// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "RemoveNotes.hpp"

#include <Midi/MidiProcess.hpp>

#include <score/model/path/PathSerialization.hpp>

namespace Midi
{

RemoveNotes::RemoveNotes(const ProcessModel& model, const std::vector<Id<Note>>& notes)
    : m_model{model}
{
  for (auto id : notes)
  {
    auto& note = model.notes.at(id);
    m_notes.push_back(
        qMakePair(id, NoteData{note.start(), note.duration(), note.pitch(), note.velocity()}));
  }
}

void RemoveNotes::undo(const score::DocumentContext& ctx) const
{
  auto& model = m_model.find(ctx);
  for (auto& note : m_notes)
  {
    model.notes.add(new Note{note.first, note.second, &model});
  }
}

void RemoveNotes::redo(const score::DocumentContext& ctx) const
{
  auto& model = m_model.find(ctx);
  for (auto& note : m_notes)
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
