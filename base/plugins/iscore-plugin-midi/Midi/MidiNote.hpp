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

        Note(const Id<Note>& id, QObject* parent);
        Note(const Id<Note>& id, NoteData n, QObject* parent);

        template<typename DeserializerVisitor,
                 enable_if_deserializer<DeserializerVisitor>* = nullptr>
        Note(DeserializerVisitor&& vis,
             QObject* parent) :
            IdentifiedObject<Note>{vis, parent}
        {
            vis.writeTo(*this);
        }

        Note* clone(const Id<Note>& id, QObject* parent);

        // Both are between 0 - 1, 1 being the process duration.
        double start() const;
        double duration() const;
        double end() const;
        midi_size_t pitch() const;
        midi_size_t velocity() const;

        void scale(double s);

        void setStart(double s);

        void setDuration(double s);

        void setPitch(midi_size_t s);

        void setVelocity(midi_size_t s);

        NoteData noteData() const;

        void setData(NoteData d);

    signals:
        void noteChanged();

    private:
        double m_start{};
        double m_duration{};

        midi_size_t m_pitch{};
        midi_size_t m_velocity{64};
};
}
