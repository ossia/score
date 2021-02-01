#pragma once
#include <ossia/audio/pulseaudio_protocol.hpp>

#include <QObject>

#include <Audio/AudioInterface.hpp>
#include <Audio/PortAudioInterface.hpp>
class QComboBox;
namespace Audio
{
#if defined(OSSIA_AUDIO_PORTAUDIO) && __has_include(<pa_linux_alsa.h>)
class ALSAFactory final : public QObject, public AudioFactory
{
  SCORE_CONCRETE("3533ee88-9a8d-486c-b20b-6c966cf4eaa0")
public:
  std::vector<PortAudioCard> devices;

  ALSAFactory();
  ~ALSAFactory() override;

  void rescan();

  bool available() const noexcept override { return true; }
  QString prettyName() const override;
  std::unique_ptr<ossia::audio_engine>
  make_engine(const Audio::Settings::Model& set, const score::ApplicationContext& ctx) override;

  void setCard(QComboBox* combo, QString val);

  QWidget* make_settings(
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent) override;
};
#endif
}

namespace Audio
{
#if defined(OSSIA_AUDIO_PULSEAUDIO)
class PulseAudioFactory final : public QObject, public AudioFactory
{
  SCORE_CONCRETE("015cb998-7bad-46e0-ba57-669b7733eadc")
public:
  PulseAudioFactory();
  ~PulseAudioFactory() override;

  bool available() const noexcept override;
  QString prettyName() const override;
  std::unique_ptr<ossia::audio_engine>
  make_engine(const Audio::Settings::Model& set, const score::ApplicationContext& ctx) override;

  QWidget* make_settings(
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent) override;
};
#endif
}
