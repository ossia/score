// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScaleNotes.hpp"

#include <Midi/MidiProcess.hpp>

#include <score/model/path/PathSerialization.hpp>

namespace Midi
{
ScaleNotes::ScaleNotes(
    const ProcessModel& model, const std::vector<Id<Note>>& to_move, double delta)
    : m_model{model}
{
  m_durations.reserve(to_move.size());
  for(auto& id : to_move)
  {
    const double before = model.notes.at(id).duration();
    m_durations.push_back({id, {before, std::max(before + delta, 0.001)}});
  }
}

void ScaleNotes::undo(const score::DocumentContext& ctx) const
{
  auto& model = m_model.find(ctx);
  for(auto& [id, d] : m_durations)
    model.notes.at(id).setDuration(d.first);
  model.notesNeedUpdate();
}

void ScaleNotes::redo(const score::DocumentContext& ctx) const
{
  auto& model = m_model.find(ctx);
  for(auto& [id, d] : m_durations)
    model.notes.at(id).setDuration(d.second);
  model.notesNeedUpdate();
}

void ScaleNotes::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_durations;
}

void ScaleNotes::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_durations;
}

RescaleMidi::RescaleMidi(const ProcessModel& model, double delta)
    : m_model{model}
    , m_delta{delta}
{
  m_old.reserve(model.notes.size());
  for(auto& note : model.notes)
    m_old.push_back({note.id(), note.noteData()});
}

void RescaleMidi::undo(const score::DocumentContext& ctx) const
{
  auto& model = m_model.find(ctx);
  model.notes.clear();
  for(auto& note : m_old)
  {
    model.notes.add(new Note{note.first, note.second, &model});
  }
}

void RescaleMidi::redo(const score::DocumentContext& ctx) const
{
  auto& model = m_model.find(ctx);
  for(auto& note : model.notes)
  {
    note.setStart(note.start() * m_delta);
    note.setDuration(note.duration() * m_delta);
  }
  model.notesNeedUpdate();
}

void RescaleMidi::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_old << m_delta;
}

void RescaleMidi::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_old >> m_delta;
}
}
