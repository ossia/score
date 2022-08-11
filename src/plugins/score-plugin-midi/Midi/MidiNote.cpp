// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MidiNote.hpp"

#include <wobjectimpl.h>
W_OBJECT_IMPL(Midi::Note)

namespace Midi
{

Note::Note(const Id<Note>& id, QObject* parent)
    : IdentifiedObject<Note>(id, QStringLiteral("Note"), parent)
{
}

Note::Note(const Id<Note>& id, NoteData n, QObject* parent)
    : IdentifiedObject<Note>(id, QStringLiteral("Note"), parent)
    , m_start{n.m_start}
    , m_duration{n.m_duration}
    , m_pitch{n.m_pitch}
    , m_velocity{n.m_velocity}
{
}

void Note::scale(double s) noexcept
{
  if(s != 1.)
  {
    m_start *= s;
    m_duration *= s;
    noteChanged();
  }
}

void Note::setStart(double s) noexcept
{
  if(m_start != s)
  {
    m_start = s;
    noteChanged();
  }
}

void Note::setDuration(double s) noexcept
{
  if(m_duration != s)
  {
    m_duration = s;
    noteChanged();
  }
}

void Note::setPitch(midi_size_t s) noexcept
{
  if(m_pitch != s)
  {
    m_pitch = s;
    noteChanged();
  }
}

void Note::setVelocity(midi_size_t s) noexcept
{
  if(m_velocity != s)
  {
    m_velocity = s;
    noteChanged();
  }
}

NoteData Note::noteData() const noexcept
{
  return NoteData{m_start, m_duration, m_pitch, m_velocity};
}

void Note::setData(NoteData d) noexcept
{
  m_start = d.m_start;
  m_duration = d.m_duration;
  m_pitch = d.m_pitch;
  m_velocity = d.m_velocity;

  noteChanged();
}
}
