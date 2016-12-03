#pragma once
#include <Midi/Commands/CommandFactory.hpp>
#include <Midi/MidiNote.hpp>
#include <iscore/tools/ModelPath.hpp>

namespace Midi
{
class ProcessModel;

class ScaleNotes final : public iscore::SerializableCommand
{
  ISCORE_COMMAND_DECL(Midi::CommandFactoryName(), ScaleNotes, "Scale notes")
public:
  ScaleNotes(
      const ProcessModel& model,
      const std::vector<Id<Note>>& to_move,
      double delta);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_model;
  std::vector<Id<Note>> m_toScale;
  double m_delta{};
};
}
