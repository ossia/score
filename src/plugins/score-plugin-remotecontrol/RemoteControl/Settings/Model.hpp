#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <score_plugin_remotecontrol_export.h>

namespace RemoteControl
{
namespace Settings
{
class SCORE_PLUGIN_REMOTECONTROL_EXPORT Model : public score::SettingsDelegateModel
{
  W_OBJECT(Model)
  bool m_Enabled{false};
  bool m_ServerEnabled{false};
  QString m_ServerAddress{"0.0.0.0"};
  unsigned short m_ServerPort{8080};

public:
  Model(
      const UuidKey<score::SettingsDelegateFactory>& k, QSettings& set,
      const score::ApplicationContext& ctx);

  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_REMOTECONTROL_EXPORT, bool, Enabled)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_REMOTECONTROL_EXPORT, bool, ServerEnabled)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_REMOTECONTROL_EXPORT, QString, ServerAddress)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_REMOTECONTROL_EXPORT, unsigned short, ServerPort)
};

SCORE_SETTINGS_PARAMETER(Model, Enabled)
SCORE_SETTINGS_PARAMETER(Model, ServerEnabled)
SCORE_SETTINGS_PARAMETER(Model, ServerAddress)
SCORE_SETTINGS_PARAMETER(Model, ServerPort)
}
}
