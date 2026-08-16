#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <score_plugin_media_export.h>

#include <verdigris>
namespace Media::Settings
{
class SCORE_PLUGIN_MEDIA_EXPORT Model : public score::SettingsDelegateModel
{
  W_OBJECT(Model)

  QStringList m_VstPaths;
  QStringList m_Vst3Paths;
  QStringList m_ClapPaths;
  QStringList m_Lv2Paths;
  bool m_VstAlwaysOnTop{};

public:
  Model(
      const UuidKey<score::SettingsDelegateFactory>& k, QSettings& set,
      const score::ApplicationContext& ctx);

  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_MEDIA_EXPORT, QStringList, VstPaths)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_MEDIA_EXPORT, QStringList, Vst3Paths)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_MEDIA_EXPORT, QStringList, ClapPaths)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_MEDIA_EXPORT, QStringList, Lv2Paths)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_MEDIA_EXPORT, bool, VstAlwaysOnTop)
};

SCORE_SETTINGS_PARAMETER(Model, VstPaths)
SCORE_SETTINGS_PARAMETER(Model, Vst3Paths)
SCORE_SETTINGS_PARAMETER(Model, ClapPaths)
SCORE_SETTINGS_PARAMETER(Model, Lv2Paths)
}
