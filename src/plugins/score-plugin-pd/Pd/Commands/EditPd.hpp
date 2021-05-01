#pragma once
#include <Pd/Commands/PdCommandFactory.hpp>
#include <Pd/PdProcess.hpp>

#include <score/command/PropertyCommand.hpp>
#include <score/model/path/Path.hpp>

#include <Scenario/Commands/ScriptEditCommand.hpp>

namespace Pd
{
class EditPdPath final
    : public Scenario::EditScript<Pd::ProcessModel, Pd::ProcessModel::p_script>
{
  SCORE_COMMAND_DECL(
      Pd::CommandFactoryName(),
      EditPdPath,
      "Edit path to Pd file")

public:
  using Scenario::EditScript<Pd::ProcessModel, Pd::ProcessModel::p_script>::
      EditScript;
};

class SetAudioIns final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(Pd::CommandFactoryName(), SetAudioIns, "Set audio ins")
public:
  SetAudioIns(const ProcessModel& path, int newval)
      : score::PropertyCommand{std::move(path), "audioInputs", newval}
  {
  }
};
class SetAudioOuts final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(Pd::CommandFactoryName(), SetAudioOuts, "Set audio outs")
public:
  SetAudioOuts(const ProcessModel& path, int newval)
      : score::PropertyCommand{std::move(path), "audioOutputs", newval}
  {
  }
};
class SetMidiIn final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(Pd::CommandFactoryName(), SetMidiIn, "Set midi in")
public:
  SetMidiIn(const ProcessModel& path, bool newval)
      : score::PropertyCommand{std::move(path), "midiInputs", newval}
  {
  }
};
class SetMidiOut final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(Pd::CommandFactoryName(), SetMidiOut, "Set midi out")
public:
  SetMidiOut(const ProcessModel& path, bool newval)
      : score::PropertyCommand{std::move(path), "midiOutputs", newval}
  {
  }
};
}
