#pragma once
#include <Midi/Commands/CommandFactory.hpp>
#include <Midi/MidiNote.hpp>
#include <iscore/tools/ModelPath.hpp>

namespace Midi
{
class ProcessModel;

class MoveNotes final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(Midi::CommandFactoryName(), MoveNotes, "Move notes")
    public:
        MoveNotes(
                const ProcessModel& model,
                const std::vector<Id<Note>>& to_move,
                int note_delta,
                double t_delta
                );

        void undo() const override;
        void redo() const override;

        void update(unused_t, unused_t, int note_delta, double t_delta);
    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<ProcessModel> m_model;
        QVector<QPair<Id<Note>, NoteData>> m_before, m_after;

};

}
