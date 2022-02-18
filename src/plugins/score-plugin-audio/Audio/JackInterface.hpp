#pragma once
#include <Audio/AudioInterface.hpp>

#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>

#include <ossia/audio/jack_protocol.hpp>
#include <ossia/dataflow/transport.hpp>

#include <score_plugin_audio_export.h>

#if defined(OSSIA_AUDIO_JACK)
Q_DECLARE_METATYPE(ossia::transport_status)
W_REGISTER_ARGTYPE(ossia::transport_status)
#endif

class QFormLayout;
namespace Audio
{
namespace Settings
{
class Model;
class View;
}
#if defined(OSSIA_AUDIO_JACK)
class SCORE_PLUGIN_AUDIO_EXPORT JackFactory final
    : public QObject
    , public AudioFactory
{
  W_OBJECT(JackFactory)
  SCORE_CONCRETE("7ff2af00-f2f5-4930-beec-0e2d21eda195")
private:
  std::weak_ptr<ossia::jack_client> m_client{};

public:
  ~JackFactory() override;

  bool available() const noexcept override;
  void initialize(
      Audio::Settings::Model& set,
      const score::ApplicationContext& ctx) override
  {
  }

  QString prettyName() const override { return QObject::tr("JACK"); }
  std::shared_ptr<ossia::audio_engine> make_engine(
      const Audio::Settings::Model& set,
      const score::ApplicationContext& ctx) override;

  std::shared_ptr<ossia::jack_client> acquireClient();

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

  void transportStateChanged(ossia::transport_status st)
  E_SIGNAL(SCORE_PLUGIN_AUDIO_EXPORT, transportStateChanged, st)

  ossia::tick_transport_info currentTransportInfo;

private:
  jack_transport_state_t m_prevState{};
};
#endif
}


