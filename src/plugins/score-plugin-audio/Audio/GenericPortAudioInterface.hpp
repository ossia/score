#pragma once
#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>

#include <ossia/audio/portaudio_protocol.hpp>

#include <Audio/AudioInterface.hpp>
#include <Audio/PortAudioInterface.hpp>
class QComboBox;
namespace Audio
{
#if defined(OSSIA_AUDIO_PORTAUDIO)

class PortAudioFactory final : public AudioFactory
{
  SCORE_CONCRETE("e7543875-3b22-457c-bf41-75504637686f")
public:
  std::vector<PortAudioCard> devices;

  PortAudioFactory();

  ~PortAudioFactory() override;
  bool available() const noexcept override { return true; }

  void rescan();

  QString prettyName() const override;
  std::unique_ptr<ossia::audio_engine>
  make_engine(const Audio::Settings::Model& set, const score::ApplicationContext& ctx) override;

  void setCardIn(QComboBox* combo, QString val);
  void setCardOut(QComboBox* combo, QString val);

  void updateSampleRates(QComboBox* rate, const PortAudioCard& input, const PortAudioCard& output);

  QWidget* make_settings(
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent) override;
};
#endif
}
