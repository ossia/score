// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MidiNote.hpp"

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

double Note::start() const
{
  return m_start;
}

double Note::duration() const
{
  return m_duration;
}

double Note::end() const
{
  return m_start + m_duration;
}

midi_size_t Note::pitch() const
{
  return m_pitch;
}

midi_size_t Note::velocity() const
{
  return m_velocity;
}

void Note::scale(double s)
{
  if (s != 1.)
  {
    m_start *= s;
    m_duration *= s;
    emit noteChanged();
  }
}

void Note::setStart(double s)
{
  if (m_start != s)
  {
    m_start = s;
    emit noteChanged();
  }
}

void Note::setDuration(double s)
{
  if (m_duration != s)
  {
    m_duration = s;
    emit noteChanged();
  }
}

void Note::setPitch(midi_size_t s)
{
  if (m_pitch != s)
  {
    m_pitch = s;
    emit noteChanged();
  }
}

void Note::setVelocity(midi_size_t s)
{
  if (m_velocity != s)
  {
    m_velocity = s;
    emit noteChanged();
  }
}

NoteData Note::noteData() const
{
  return NoteData{m_start, m_duration, m_pitch, m_velocity};
}

void Note::setData(NoteData d)
{
  m_start = d.m_start;
  m_duration = d.m_duration;
  m_pitch = d.m_pitch;
  m_velocity = d.m_velocity;

  emit noteChanged();
}
}
