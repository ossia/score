#pragma once
#include <Audio/AudioInterface.hpp>

#include <ossia/audio/alsa_protocol.hpp>

#include <QObject>

#include <map>
class QComboBox;
namespace Audio
{
#if defined(OSSIA_AUDIO_ALSA)
struct range
{
  int min;
  int max;
};

//! What one direction of a PCM accepts, as the engine will open it.
struct AlsaStream
{
  bool present{};
  //! Another program, or the running engine, has it open: the capabilities
  //! are the ones seen when it was last free, if ever.
  bool busy{};
  range channels{};
  std::vector<int> rates{};
  std::map<int, std::vector<int>> buffer_sizes{};
};

struct AlsaCard
{
  //! hw:CARD=<id>,DEV=<n>: stable when cards enumerate in another order.
  QString raw_name;
  QString pretty_name;
  //! plughw: and the like: converts formats and rates, adds latency.
  bool converted{};

  AlsaStream playback;
  AlsaStream capture;
};

class ALSAFactory final
    : public QObject
    , public AudioFactory
{
  SCORE_CONCRETE("a390218a-a951-4cda-b4ee-c41d2df44236")
public:
  std::vector<AlsaCard> devices;

  ALSAFactory();
  ~ALSAFactory() override;

  void
  initialize(Audio::Settings::Model& set, const score::ApplicationContext& ctx) override;
  void rescan();
  const AlsaCard* find(const QString& raw_name) const noexcept;
  //! The first hardware card that can play and is free.
  const AlsaCard* defaultCard() const noexcept;

  bool available() const noexcept override;
  QString prettyName() const override;
  std::shared_ptr<ossia::audio_engine> make_engine(
      const Audio::Settings::Model& set, const score::ApplicationContext& ctx) override;

  QWidget* make_settings(
      Audio::Settings::Model& m, Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp, QWidget* parent) override;

private:
  std::map<QString, AlsaCard> m_lastFree;
  bool m_scanned{};
};
#endif
}
