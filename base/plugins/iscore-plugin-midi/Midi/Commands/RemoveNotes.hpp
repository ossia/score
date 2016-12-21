#pragma once
#include <Midi/Commands/CommandFactory.hpp>
#include <Midi/MidiNote.hpp>
#include <iscore/model/path/Path.hpp>

namespace Midi
{
class ProcessModel;

class RemoveNotes final : public iscore::Command
{
  ISCORE_COMMAND_DECL(Midi::CommandFactoryName(), RemoveNotes, "Remove notes")
public:
  RemoveNotes(const ProcessModel& model, const std::vector<Id<Note>>& to_move);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_model;
  QVector<QPair<Id<Note>, NoteData>> m_notes;
};
}
