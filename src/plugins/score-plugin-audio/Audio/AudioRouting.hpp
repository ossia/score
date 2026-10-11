#pragma once
#include <State/Address.hpp>

#include <Audio/PortGain.hpp>

#include <score/command/AggregateCommand.hpp>

#include <ossia/detail/pod_vector.hpp>

#include <score_plugin_audio_export.h>

#include <string>
#include <utility>
#include <vector>

class QWidget;
namespace score
{
struct DocumentContext;
}
namespace Dataflow
{
class AudioDevice;
}

namespace Audio
{
class SCORE_PLUGIN_AUDIO_EXPORT ChangeRouting final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      Audio::CommandFactoryName(), ChangeRouting, "Change the audio routing")
};

using Routes = std::vector<std::pair<State::Address, ossia::int_vector>>;

SCORE_PLUGIN_AUDIO_EXPORT
void applyRouting(const score::DocumentContext& ctx, Routes routes);

SCORE_PLUGIN_AUDIO_EXPORT
void addAudioPort(
    const score::DocumentContext& ctx, Dataflow::AudioDevice& dev, QWidget* parent,
    const std::string& kind = "in");

SCORE_PLUGIN_AUDIO_EXPORT
void showAudioRouting(const score::DocumentContext& ctx);
}
