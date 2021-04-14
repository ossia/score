#pragma once
#include <Audio/AudioInterface.hpp>

#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>

#include <ossia/audio/jack_protocol.hpp>

class QFormLayout;
namespace Audio
{
namespace Settings {
class Model;
class View;
}
#if defined(OSSIA_AUDIO_JACK)
class JackFactory final : public AudioFactory
{
  SCORE_CONCRETE("7ff2af00-f2f5-4930-beec-0e2d21eda195")
private:
  std::weak_ptr<ossia::jack_client> m_client{};

public:
  ~JackFactory() override;

  bool available() const noexcept override;

  QString prettyName() const override { return QObject::tr("JACK"); }
  std::unique_ptr<ossia::audio_engine>
  make_engine(const Audio::Settings::Model& set, const score::ApplicationContext& ctx) override;

  void setupSettingsWidget(
      QWidget* w,
      QFormLayout* lay,
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp);
  QWidget* make_settings(
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent) override;
};
#endif
}
