#pragma once
#include <Midi/Commands/CommandFactory.hpp>
#include <Midi/MidiNote.hpp>
#include <iscore/tools/ModelPath.hpp>

namespace Midi
{
class ProcessModel;

class AddNote final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(Midi::CommandFactoryName(), AddNote, "Add a note")
    public:
        AddNote(
                const ProcessModel& model,
                const NoteData& note);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<ProcessModel> m_model;
        Id<Note> m_id;
        NoteData m_note;

};

}
