#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <wobjectdefs.h>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score_plugin_engine_export.h>
namespace Audio::Settings
{
class SCORE_PLUGIN_ENGINE_EXPORT Model : public score::SettingsDelegateModel
{
  W_OBJECT(Model)

  QString m_Driver{};
  QString m_CardIn{};
  QString m_CardOut{};
  int m_BufferSize{};
  int m_Rate{};
  int m_DefaultIn{};
  int m_DefaultOut{};

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, QString, Driver)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, QString, CardIn)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, QString, CardOut)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, int, BufferSize)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, int, Rate)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, int, DefaultIn)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, int, DefaultOut)
};

SCORE_SETTINGS_PARAMETER(Model, Driver)
SCORE_SETTINGS_PARAMETER(Model, CardIn)
SCORE_SETTINGS_PARAMETER(Model, CardOut)
SCORE_SETTINGS_PARAMETER(Model, BufferSize)
SCORE_SETTINGS_PARAMETER(Model, Rate)
SCORE_SETTINGS_PARAMETER(Model, DefaultIn)
SCORE_SETTINGS_PARAMETER(Model, DefaultOut)
}
