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
  bool m_VstAlwaysOnTop{};

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_MEDIA_EXPORT, QStringList, VstPaths)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_MEDIA_EXPORT, bool, VstAlwaysOnTop)
};

SCORE_SETTINGS_PARAMETER(Model, VstPaths)
}
