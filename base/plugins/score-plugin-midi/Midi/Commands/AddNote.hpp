#pragma once
#include <Midi/Commands/CommandFactory.hpp>
#include <Midi/MidiNote.hpp>
#include <score/model/path/Path.hpp>
#include <Process/TimeValue.hpp>
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


class ReplaceNotes final : public score::Command
{
  SCORE_COMMAND_DECL(Midi::CommandFactoryName(), ReplaceNotes, "Set notes")
public:
  ReplaceNotes(const ProcessModel& model, const std::vector<NoteData>& note, int min, int max, TimeVal dur);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_model;
  std::vector<std::pair<Id<Note>, NoteData>> m_old, m_new;
  int m_oldmin{}, m_oldmax{}, m_newmin{}, m_newmax{};
  TimeVal m_olddur{}, m_newdur{};
};
}
