#pragma once
#include <Audio/PortAudioInterface.hpp>
#include <Audio/AudioInterface.hpp>
#include <QObject>
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

  QString prettyName() const override;
  std::unique_ptr<ossia::audio_engine> make_engine(
      const Audio::Settings::Model& set,
      const score::ApplicationContext& ctx) override;

  void setCard(QComboBox* combo, QString val);

  QWidget* make_settings(
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent) override;
};
#endif
}
