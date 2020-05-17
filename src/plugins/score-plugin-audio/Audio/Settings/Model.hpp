#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <Audio/AudioInterface.hpp>
#include <score_plugin_audio_export.h>

#include <verdigris>

namespace Audio::Settings
{

class SCORE_PLUGIN_AUDIO_EXPORT Model : public score::SettingsDelegateModel
{
  W_OBJECT(Model)

  Audio::AudioFactory::ConcreteKey m_Driver{};
  QString m_CardIn{};
  QString m_CardOut{};
  int m_BufferSize{};
  int m_Rate{};
  int m_DefaultIn{};
  int m_DefaultOut{};
  bool m_AutoStereo{true};

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  void changed() E_SIGNAL(SCORE_PLUGIN_AUDIO_EXPORT, changed)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, Audio::AudioFactory::ConcreteKey, Driver)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, QString, CardIn)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, QString, CardOut)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, int, BufferSize)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, int, Rate)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, int, DefaultIn)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, int, DefaultOut)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_AUDIO_EXPORT, bool, AutoStereo)
};

SCORE_SETTINGS_PARAMETER(Model, Driver)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, CardIn)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, CardOut)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, BufferSize)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, Rate)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, DefaultIn)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, DefaultOut)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, AutoStereo)
}

#undef AUDIO_PARAMETER_HPP
