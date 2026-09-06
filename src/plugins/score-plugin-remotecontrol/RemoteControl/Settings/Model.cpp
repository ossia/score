#include "Model.hpp"

#include <Library/LibrarySettings.hpp>

#include <QSettings>
#include <QDir>
#include <QUuid>
#include <score/application/ApplicationContext.hpp>

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
SETTINGS_PARAMETER_IMPL(ServerPort){QStringLiteral("RemoteControl/ServerPort"), 10111};
SETTINGS_PARAMETER_IMPL(ServerEnabled){QStringLiteral("RemoteControl/ServerEnabled"), false};
SETTINGS_PARAMETER_IMPL(Token){QStringLiteral("RemoteControl/Token"), QString{}};
SETTINGS_PARAMETER_IMPL(AllowScripting){
    QStringLiteral("RemoteControl/AllowScripting"), false};
static auto list()
{
  return std::tie(Enabled
                  , WebUiPath
                  , ServerAddress
                  , ServerPort
                  , ServerEnabled
                  , Token
                  , AllowScripting);
}
}

Model::Model(
    const UuidKey<score::SettingsDelegateFactory>& k, QSettings& set,
    const score::ApplicationContext& ctx)
    : score::SettingsDelegateModel{k, nullptr}
{
  score::setupDefaultSettings(set, Parameters::list(), *this);

  if (m_WebUiPath.isEmpty())
  {
    const QString path{score::AppContext()
      .settings<Library::Settings::Model>()
              .getPackagesPath()
          + "/wasm-remote/"};

    if (QDir{path}.exists())
      setWebUiPath(path);
  }

  if(m_Token.isEmpty())
    setToken(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

SCORE_SETTINGS_PARAMETER_CPP(bool, Model, Enabled)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, WebUiPath)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, ServerAddress)
SCORE_SETTINGS_PARAMETER_CPP(unsigned short, Model, ServerPort)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, ServerEnabled)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, Token)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, AllowScripting)
}
}
