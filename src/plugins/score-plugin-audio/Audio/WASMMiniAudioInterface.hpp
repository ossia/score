#pragma once
#if defined(__EMSCRIPTEN__)
#include <Audio/AudioInterface.hpp>
#include <Audio/Settings/Model.hpp>

#include <ossia/audio/miniaudio_protocol.hpp>

#if OSSIA_ENABLE_MINIAUDIO

namespace Audio
{

class WASMMiniAudioFactory final : public AudioFactory
{
  SCORE_CONCRETE("a2e8b688-521c-4755-b0b2-4e3d01807b8e")
public:
  ~WASMMiniAudioFactory() override { }
  bool available() const noexcept override { return true; }
  void
  initialize(Audio::Settings::Model& set, const score::ApplicationContext& ctx) override
  {
  }

  QString prettyName() const override { return QObject::tr("Web Audio (MiniAudio)"); }

  std::shared_ptr<ossia::audio_engine> make_engine(
      const Audio::Settings::Model& set, const score::ApplicationContext& ctx) override
  {
    auto context = std::make_shared<ossia::miniaudio_context>();
    auto cfg = ma_context_config_init();
    ma_context_init(nullptr, 0, &cfg, &context->context);

    ma_device_id in_id{}, out_id{};

    // Request microphone input so the miniaudio WebAudio backend opens a
    // duplex device (ma_device_type_duplex) and calls getUserMedia(). With 0
    // inputs it would be playback-only and capture would never be requested.
    // The browser prompts for mic permission on the first duplex device; the
    // first buffers are silent until the user grants it.
    int inputs = set.getDefaultIn();
    if(inputs <= 0)
      inputs = 2;
    int outputs = set.getDefaultOut();
    if(outputs <= 0)
      outputs = 2;

    return std::make_shared<ossia::miniaudio_engine>(
        std::move(context), "ossia score", in_id, out_id, inputs, outputs, 48000, 1024);
  }

  QWidget* make_settings(
      Audio::Settings::Model& m, Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp, QWidget* parent) override
  {
    return nullptr;
  }
};

}
#endif
#endif
