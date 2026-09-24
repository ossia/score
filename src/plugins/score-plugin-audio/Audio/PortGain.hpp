#pragma once
#include <State/Address.hpp>

#include <score/command/AggregateCommand.hpp>
#include <score/command/Command.hpp>

#include <score_plugin_audio_export.h>

namespace score
{
struct DocumentContext;
}
namespace Audio
{
SCORE_PLUGIN_AUDIO_EXPORT
const CommandGroupKey& CommandFactoryName();

//! The gain of a port of the audio device. It is kept in the document with
//! the port's node, and restored when the document loads.
class SCORE_PLUGIN_AUDIO_EXPORT SetPortGain final : public score::Command
{
  SCORE_COMMAND_DECL(Audio::CommandFactoryName(), SetPortGain, "Set the gain of a port")
public:
  SetPortGain(State::Address address, double before, double after);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  void apply(const score::DocumentContext& ctx, double gain) const;

  State::Address m_address;
  double m_before{};
  double m_after{};
};

//! Shows the audio device in the device explorer, whose nodes the document
//! saves, and sets the gain of one of its ports.
class SCORE_PLUGIN_AUDIO_EXPORT SetPortGainMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(Audio::CommandFactoryName(), SetPortGainMacro, "Set the gain of a port")
};

//! Records a gain the port already has, set from `before`: one undo step.
SCORE_PLUGIN_AUDIO_EXPORT
void commitPortGain(
    const score::DocumentContext& ctx, const State::Address& address, double before,
    double after);
}
