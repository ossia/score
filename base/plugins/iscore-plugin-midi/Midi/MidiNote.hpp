#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/selection/Selectable.hpp>

namespace Midi
{
using midi_size_t = uint8_t;
struct NoteData
{
        NoteData() = default;
        NoteData(double s, double d, midi_size_t p, midi_size_t v):
            start{s}, duration{d}, pitch{p}, velocity{v} { }

        double start{};
        double duration{};

        midi_size_t pitch{};
        midi_size_t velocity{};
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
class Note : public IdentifiedObject<Note>
{
        Q_OBJECT

    public:
        Selectable selection;

        Note(const Id<Note>& id, QObject* parent):
            IdentifiedObject<Note>(id, "Note", parent)
        {
        }

        Note(const Id<Note>& id, NoteData n, QObject* parent):
            IdentifiedObject<Note>(id, "Note", parent),
            m_start{n.start},
            m_duration{n.duration},
            m_pitch{n.pitch},
            m_velocity{n.velocity}
        {
        }

        Note* clone(const Id<Note>& id, QObject* parent)
        {
            return new Note{id, this->nodeData(), parent};
        }

        // Both are between 0 - 1, 1 being the process duration.
        double start() const { return m_start; }
        double duration() const { return m_duration; }
        double end() const { return m_start + m_duration; }
        midi_size_t pitch() const { return m_pitch; }
        midi_size_t velocity() const { return m_velocity; }

        void scale(double s)
        {
            m_start *= s;
            m_duration *= s;
            emit noteChanged();
        }

        void setStart(double s)
        {
            m_start = s;
            emit noteChanged();
        }

        void setDuration(double s)
        {
            m_duration = s;
            emit noteChanged();
        }

        void setPitch(midi_size_t s)
        {
            m_pitch = s;
            emit noteChanged();
        }

        void setVelocity(midi_size_t s)
        {
            m_velocity = s;
            emit noteChanged();
        }

        NoteData nodeData() const
        {
            return NoteData{
                m_start, m_duration,
                m_pitch, m_velocity};
        }

    signals:
        void noteChanged();

    private:
        double m_start{};
        double m_duration{};

        midi_size_t m_pitch{};
        midi_size_t m_velocity{64};
};
}
