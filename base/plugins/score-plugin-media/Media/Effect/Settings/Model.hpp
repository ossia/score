#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <score_plugin_media_export.h>
namespace Media::Settings
{
class SCORE_PLUGIN_MEDIA_EXPORT Model
    : public score::SettingsDelegateModel
{
  Q_OBJECT
    Q_PROPERTY(QStringList VstPaths READ getVstPaths WRITE setVstPaths NOTIFY VstPathsChanged)
  QStringList m_VstPaths;

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);


  SCORE_SETTINGS_PARAMETER_HPP(QStringList, VstPaths)
};

SCORE_SETTINGS_PARAMETER(Model, VstPaths)
}


