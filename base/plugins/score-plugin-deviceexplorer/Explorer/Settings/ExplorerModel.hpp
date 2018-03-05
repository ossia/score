#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <score_plugin_deviceexplorer_export.h>
namespace Explorer::Settings
{
struct DeviceLogLevel
{
  QString logNothing{"Nothing"};
  QString logUnfolded{"Unfolded"};
  QString logEverything{"Everything"};
  operator QStringList() const {
    return { logNothing, logUnfolded, logEverything };
  }
};

class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT Model 
    : public score::SettingsDelegateModel
{
  Q_OBJECT
  Q_PROPERTY(bool LocalTree READ getLocalTree WRITE setLocalTree NOTIFY LocalTreeChanged)
  Q_PROPERTY(QString logLevel READ getLogLevel WRITE setLogLevel NOTIFY LogLevelChanged)
  bool m_LocalTree = false;
  QString m_LogLevel;

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  SCORE_SETTINGS_PARAMETER_HPP(bool, LocalTree)
  SCORE_SETTINGS_PARAMETER_HPP(QString, LogLevel)
};

SCORE_SETTINGS_PARAMETER(Model, LogLevel)
SCORE_SETTINGS_DEFERRED_PARAMETER(Model, LocalTree)
}


namespace Explorer::ProjectSettings
{ class Model; }

UUID_METADATA(
    ,
    score::DocumentPluginFactory,
    Explorer::ProjectSettings::Model,
    "1f923578-08c3-49be-9ba9-69c144ee2e32")

namespace Explorer::ProjectSettings
{
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT Model
    : public score::ProjectSettingsModel
{
  Q_OBJECT
  SCORE_SERIALIZE_FRIENDS
  MODEL_METADATA_IMPL(Model)

  Q_PROPERTY(bool RefreshOnStart READ getRefreshOnStart WRITE setRefreshOnStart NOTIFY RefreshOnStartChanged)
  Q_PROPERTY(bool ReconnectOnStart READ getReconnectOnStart WRITE setReconnectOnStart NOTIFY ReconnectOnStartChanged)
  bool m_RefreshOnStart = false;
  bool m_ReconnectOnStart = false;

public:
  Model(const score::DocumentContext&,
        Id<DocumentPlugin> id,
        QObject* parent);

  template <typename Impl>
  Model(
      const score::DocumentContext& ctx,
      Impl& vis,
      QObject* parent)
      : score::ProjectSettingsModel{ctx, vis, parent}
  {
    vis.writeTo(*this);
  }

  SCORE_SETTINGS_PARAMETER_HPP(bool, RefreshOnStart)
  SCORE_SETTINGS_PARAMETER_HPP(bool, ReconnectOnStart)
};

SCORE_SETTINGS_PARAMETER(Model, RefreshOnStart)
SCORE_SETTINGS_PARAMETER(Model, ReconnectOnStart)

}
