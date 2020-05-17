// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddNote.hpp"

#include <Midi/MidiProcess.hpp>
#include <Process/TimeValueSerialization.hpp>

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

ReplaceNotes::ReplaceNotes(
    const ProcessModel& model,
    const std::vector<NoteData>& n,
    int min,
    int max,
    TimeVal d)
    : m_model{model}
    , m_oldmin{model.range().first}
    , m_oldmax{model.range().second}
    , m_newmin{min}
    , m_newmax{max}
    , m_olddur{model.duration()}
    , m_newdur{d}
{
  for (Note& note : model.notes)
  {
    m_old.push_back({note.id(), note.noteData()});
  }

  int i = 0;
  for (auto& note : n)
    m_new.push_back({Id<Midi::Note>{i++}, note});
}

void ReplaceNotes::undo(const score::DocumentContext& ctx) const
{
  auto& model = m_model.find(ctx);
  model.notes.clear();
  model.setDuration(m_olddur);

  for (auto& note : m_old)
    model.notes.add(new Note{note.first, note.second, &model});

  model.setRange(m_oldmin, m_oldmax);
}

void ReplaceNotes::redo(const score::DocumentContext& ctx) const
{
  auto& model = m_model.find(ctx);
  model.notes.clear();
  model.setDuration(m_newdur);

  for (auto& note : m_new)
    model.notes.add(new Note{note.first, note.second, &model});

  model.setRange(m_newmin, m_newmax);
}

void ReplaceNotes::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_old << m_new << m_oldmin << m_oldmax << m_newmin << m_newmax << m_olddur
    << m_newdur;
}

void ReplaceNotes::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_old >> m_new >> m_oldmin >> m_oldmax >> m_newmin >> m_newmax >> m_olddur
      >> m_newdur;
}
}
