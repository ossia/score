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
    return std::make_shared<ossia::miniaudio_engine>(
        std::move(context), "ossia score", in_id, out_id, 0, 2, 48000, 1024);
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
