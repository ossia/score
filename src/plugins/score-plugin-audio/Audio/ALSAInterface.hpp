#pragma once
#include <Audio/AudioInterface.hpp>

#include <ossia/audio/alsa_protocol.hpp>

#include <QObject>
class QComboBox;
namespace Audio
{
#if defined(OSSIA_AUDIO_ALSA)

struct range {
  int min;
  int max;
};
struct AlsaCard
{
  QString raw_name;
  QString pretty_name;

  range inputRange{};
  range outputRange{};

  std::vector<double> rates{};
  std::vector<int> buffer_sizes{};
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

  void initialize(
      Audio::Settings::Model& set,
      const score::ApplicationContext& ctx) override;
  void rescan();

  bool available() const noexcept override;
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
