#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

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

class Model : public score::SettingsDelegateModel
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
