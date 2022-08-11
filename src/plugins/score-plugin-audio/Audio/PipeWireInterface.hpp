#pragma once
#include <Audio/AudioInterface.hpp>

#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>

#include <ossia/audio/pipewire_protocol.hpp>

class QFormLayout;
#if defined(OSSIA_AUDIO_PIPEWIRE)
namespace ossia
{
struct pipewire_context;
}

class QSocketNotifier;
namespace Audio
{

class PipeWireAudioFactory final
    : public QObject
    , public AudioFactory
{
  SCORE_CONCRETE("687d49cf-b58d-430f-8358-ec02cb50be36")

public:
  PipeWireAudioFactory();
  ~PipeWireAudioFactory() override;

  bool available() const noexcept override;
  void
  initialize(Audio::Settings::Model& set, const score::ApplicationContext& ctx) override;
  void rescan();

  QString prettyName() const override;
  std::shared_ptr<ossia::audio_engine> make_engine(
      const Audio::Settings::Model& set, const score::ApplicationContext& ctx) override;

  void setupSettingsWidget(
      QWidget* w, QFormLayout* lay, Audio::Settings::Model& m, Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp);
  QWidget* make_settings(
      Audio::Settings::Model& m, Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp, QWidget* parent) override;

private:
  std::shared_ptr<ossia::pipewire_context> m_client{};
  QSocketNotifier* m_fd{};
};
}
#endif
