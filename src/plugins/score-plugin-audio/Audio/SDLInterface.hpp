#pragma once
#include <ossia/audio/sdl_protocol.hpp>

#include <QWidget>

#include <Audio/AudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>

namespace Audio
{

#if defined(OSSIA_AUDIO_SDL)
class SDLFactory final : public AudioFactory
{
  SCORE_CONCRETE("28b88e91-c5f0-4f13-834f-aa333d14aa81")
public:
  ~SDLFactory() override { }
  bool available() const noexcept override { return true; }

  QString prettyName() const override { return QObject::tr("SDL"); };
  std::unique_ptr<ossia::audio_engine>
  make_engine(const Audio::Settings::Model& set, const score::ApplicationContext& ctx) override
  {
    int rate = set.getRate();
    int bs = set.getBufferSize();
#if defined(__EMSCRIPTEN__)
    rate = 48000;
    bs = 1024;
#endif
    return std::make_unique<ossia::sdl_protocol>(rate, bs);
  }

  QWidget* make_settings(
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent) override
  {
    return nullptr;
  }
};
#endif
}
