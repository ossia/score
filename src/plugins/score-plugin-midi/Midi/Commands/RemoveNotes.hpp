#pragma once
#include <Midi/Commands/CommandFactory.hpp>
#include <Midi/MidiNote.hpp>

#include <score/model/path/Path.hpp>
#include <QVector>
#include <QPair>

namespace Midi
{
class ProcessModel;

class RemoveNotes final : public score::Command
{
  SCORE_COMMAND_DECL(Midi::CommandFactoryName(), RemoveNotes, "Remove notes")
public:
  RemoveNotes(const ProcessModel& model, const std::vector<Id<Note>>& to_move);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_model;
  QVector<QPair<Id<Note>, NoteData>> m_notes;
};
}
