#pragma once
#include <score/model/IdentifiedObject.hpp>
#include <score/selection/Selectable.hpp>

#include <verdigris>

namespace Midi
{
using midi_size_t = uint8_t;
struct NoteData
{
  NoteData() = default;
  NoteData(double s, double d, midi_size_t p, midi_size_t v)
      : m_start{s}, m_duration{d}, m_pitch{p}, m_velocity{v}
  {
  }

  double start() const { return m_start; }
  double duration() const { return m_duration; }
  double end() const { return m_start + m_duration; }

  midi_size_t pitch() const { return m_pitch; }
  midi_size_t velocity() const { return m_velocity; }

  void setStart(double s) { m_start = s; }
  void setDuration(double s) { m_duration = s; }
  void setPitch(midi_size_t s) { m_pitch = s; }
  void setVelocity(midi_size_t s) { m_velocity = s; }

  double m_start{};
  double m_duration{};

  midi_size_t m_pitch{};
  midi_size_t m_velocity{};
};

struct NoteComparator
{
  bool operator()(const NoteData& lhs, const NoteData& rhs) const
  {
    return lhs.m_start < rhs.m_start;
  }
  bool operator()(const NoteData& lhs, double rhs) const
  {
    return lhs.m_start < rhs;
  }
};

/**
 * @brief The Note class
 *
 * TODO rethink the object model.
 * Add a virtual findChild method to Entity.
 * Remove all uses of NamedObject and add IDs everywhere.
 * Allocate the Notes on a vector<Note> and allow them to be copied.
 * Don't allow signals in some way.
 */
class Note final : public IdentifiedObject<Note>
{
  W_OBJECT(Note)
  SCORE_SERIALIZE_FRIENDS

public:
  Selectable selection;

  Note(const Id<Note>& id, QObject* parent);
  Note(const Id<Note>& id, NoteData n, QObject* parent);

  template <
      typename DeserializerVisitor,
      enable_if_deserializer<DeserializerVisitor>* = nullptr>
  Note(DeserializerVisitor&& vis, QObject* parent)
      : IdentifiedObject<Note>{vis, parent}
  {
    vis.writeTo(*this);
  }

  // Both are between 0 - 1, 1 being the process duration.

  double start() const noexcept { return m_start; }

  double duration() const noexcept { return m_duration; }

  double end() const noexcept { return m_start + m_duration; }

  midi_size_t pitch() const noexcept { return m_pitch; }

  midi_size_t velocity() const noexcept { return m_velocity; }

  void scale(double s) noexcept;

  void setStart(double s) noexcept;

  void setDuration(double s) noexcept;

  void setPitch(midi_size_t s) noexcept;

  void setVelocity(midi_size_t s) noexcept;

  NoteData noteData() const noexcept;

  void setData(NoteData d) noexcept;

public:
  void noteChanged() W_SIGNAL(noteChanged);

private:
  double m_start{};
  double m_duration{};

  midi_size_t m_pitch{};
  midi_size_t m_velocity{64};
};
}
