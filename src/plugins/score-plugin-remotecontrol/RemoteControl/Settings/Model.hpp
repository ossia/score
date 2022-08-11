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
  bool m_Enabled = false;

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_REMOTECONTROL_EXPORT, bool, Enabled)
};

SCORE_SETTINGS_PARAMETER(Model, Enabled)

}
}
