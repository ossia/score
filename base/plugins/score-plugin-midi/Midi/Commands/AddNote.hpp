#pragma once
#include <Midi/Commands/CommandFactory.hpp>
#include <Midi/MidiNote.hpp>
#include <score/model/path/Path.hpp>

namespace Midi
{
class ProcessModel;

class AddNote final : public score::Command
{
  SCORE_COMMAND_DECL(Midi::CommandFactoryName(), AddNote, "Add a note")
public:
  AddNote(const ProcessModel& model, const NoteData& note);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_model;
  Id<Note> m_id;
  NoteData m_note;
};
}
