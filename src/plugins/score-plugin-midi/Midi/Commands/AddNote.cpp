// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddNote.hpp"

#include <Process/TimeValueSerialization.hpp>

#include <Midi/MidiProcess.hpp>

#include <score/model/path/PathSerialization.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/detail/ssize.hpp>

namespace Midi
{

AddNote::AddNote(const ProcessModel& model, const NoteData& n)
    : m_model{model}
    , m_id{getStrongId(model.notes)}
    , m_note{n}
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

AddNotes::AddNotes(const ProcessModel& model, const std::vector<NoteData>& notes)
    : m_model{model}
    , m_ids{getStrongIdRange<Note>(notes.size(), model.notes)}
    , m_notes(notes)
{
}

void AddNotes::undo(const score::DocumentContext& ctx) const
{
  for(int i = 0; i < std::ssize(m_ids); i++)
  {
    m_model.find(ctx).notes.remove(m_ids[i]);
  }
}

void AddNotes::redo(const score::DocumentContext& ctx) const
{
  auto& model = m_model.find(ctx);
  for(int i = 0; i < std::ssize(m_ids); i++)
  {
    model.notes.add(new Note{m_ids[i], m_notes[i], &model});
  }
}

void AddNotes::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_ids << m_notes;
}

void AddNotes::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_ids >> m_notes;
}

ReplaceNotes::ReplaceNotes(
    const ProcessModel& model, const std::vector<NoteData>& n, int min, int max,
    TimeVal d)
    : m_model{model}
    , m_oldmin{model.range().first}
    , m_oldmax{model.range().second}
    , m_newmin{min}
    , m_newmax{max}
    , m_olddur{model.duration()}
    , m_newdur{d}
{
  for(Note& note : model.notes)
  {
    m_old.push_back({note.id(), note.noteData()});
  }

  int i = 0;
  for(auto& note : n)
    m_new.push_back({Id<Midi::Note>{i++}, note});
}

void ReplaceNotes::undo(const score::DocumentContext& ctx) const
{
  auto& model = m_model.find(ctx);
  model.notes.clear();
  model.setDuration(m_olddur);

  IdContainer<Note> new_notes;
  for(auto& note : m_old)
    new_notes.insert(new Note{note.first, note.second, &model});
  model.notes.replace(std::move(new_notes));

  model.setRange(m_oldmin, m_oldmax);
}

void ReplaceNotes::redo(const score::DocumentContext& ctx) const
{
  auto& model = m_model.find(ctx);
  model.notes.clear();
  model.setDuration(m_newdur);

  IdContainer<Note> new_notes;
  for(auto& note : m_new)
    new_notes.insert(new Note{note.first, note.second, &model});
  model.notes.replace(std::move(new_notes));

  model.setRange(m_newmin, m_newmax);
}

void ReplaceNotes::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_old << m_new << m_oldmin << m_oldmax << m_newmin << m_newmax
    << m_olddur << m_newdur;
}

void ReplaceNotes::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_old >> m_new >> m_oldmin >> m_oldmax >> m_newmin >> m_newmax
      >> m_olddur >> m_newdur;
}
}
