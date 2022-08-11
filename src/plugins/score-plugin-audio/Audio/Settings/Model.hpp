#pragma once
#include <Audio/AudioInterface.hpp>

#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <score_plugin_audio_export.h>

#include <verdigris>

namespace Audio::Settings
{

enum class ExternalTransport : int8_t
{
  None = 0,
  Client = 1,
  Master = 2
};

class SCORE_PLUGIN_AUDIO_EXPORT Model : public score::SettingsDelegateModel
{
  W_OBJECT(Model)

  // JACK, MME, WASAPI, ALSA...
  Audio::AudioFactory::ConcreteKey m_Driver{};

  // For APIs that support setting a name to the inputs / outputs (e.g. JACK)
  QStringList m_InputNames;
  QStringList m_OutputNames;

  // Which device to use for input / output
  QString m_CardIn{};
  QString m_CardOut{};
  int m_BufferSize{};
  int m_Rate{};

  // How many i/o to create if possible
  int m_DefaultIn{};
  int m_DefaultOut{};

  // When playing mono audio files, put them as stereo automatically
  bool m_AutoStereo{true};

  // Auto connect ports to system i/o (mostly relevant for jack)
  bool m_AutoConnect{true};

  // Use JACK Transport
  ExternalTransport m_JackTransport{ExternalTransport::None};

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  void changed() E_SIGNAL(SCORE_PLUGIN_AUDIO_EXPORT, changed)
  SCORE_SETTINGS_PARAMETER_HPP(
      SCORE_PLUGIN_AUDIO_EXPORT, Audio::AudioFactory::ConcreteKey, Driver)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, QStringList, InputNames)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, QStringList, OutputNames)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, QString, CardIn)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, QString, CardOut)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, int, BufferSize)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, int, Rate)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, int, DefaultIn)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, int, DefaultOut)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, bool, AutoStereo)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, bool, AutoConnect)
  SCORE_SETTINGS_PARAMETER_HPP(
      SCORE_PLUGIN_AUDIO_EXPORT, Audio::Settings::ExternalTransport, JackTransport)
};

SCORE_SETTINGS_PARAMETER(Model, Driver)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, InputNames)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, OutputNames)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, CardIn)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, CardOut)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, BufferSize)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, Rate)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, DefaultIn)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, DefaultOut)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, AutoStereo)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, AutoConnect)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, JackTransport)
}

Q_DECLARE_METATYPE(Audio::Settings::ExternalTransport)
W_REGISTER_ARGTYPE(Audio::Settings::ExternalTransport)
#undef AUDIO_PARAMETER_HPP
