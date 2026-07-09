#include "Model.hpp"

#include <QSettings>

#include <wobjectimpl.h>
W_OBJECT_IMPL(RemoteControl::Settings::Model)
namespace RemoteControl
{
namespace Settings
{

namespace Parameters
{
SETTINGS_PARAMETER_IMPL(Enabled){QStringLiteral("RemoteControl/Enabled"), false};
SETTINGS_PARAMETER_IMPL(WebUiPath){QStringLiteral("RemoteControl/WebUiPath"), ""};
SETTINGS_PARAMETER_IMPL(ServerAddress){QStringLiteral("RemoteControl/ServerAddress"), "0.0.0.0"};
SETTINGS_PARAMETER_IMPL(ServerPort){QStringLiteral("RemoteControl/ServerPort"), 8080};
SETTINGS_PARAMETER_IMPL(ServerEnabled){QStringLiteral("RemoteControl/ServerEnabled"), false};
static auto list()
{
  return std::tie(Enabled
                  , WebUiPath
                  , ServerAddress
                  , ServerPort
                  , ServerEnabled);
}
}

Model::Model(
    const UuidKey<score::SettingsDelegateFactory>& k, QSettings& set,
    const score::ApplicationContext& ctx)
    : score::SettingsDelegateModel{k, nullptr} 
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

SCORE_SETTINGS_PARAMETER_CPP(bool, Model, Enabled)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, WebUiPath)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, ServerAddress)
SCORE_SETTINGS_PARAMETER_CPP(unsigned short, Model, ServerPort)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, ServerEnabled)
}
}
