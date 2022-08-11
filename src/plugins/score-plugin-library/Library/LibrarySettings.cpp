#include <Library/LibrarySettings.hpp>

#include <score/command/Command.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/SettingsCommand.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/FormWidget.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <QDir>
#include <QFormLayout>
#include <QLineEdit>
#include <QStandardPaths>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Library::Settings::View)
W_OBJECT_IMPL(Library::Settings::Model)

namespace Library::Settings
{

namespace Parameters
{
SETTINGS_PARAMETER_IMPL(RootPath){QStringLiteral("Library/RootPath"), []() -> QString {
                                    auto paths = QStandardPaths::standardLocations(
                                        QStandardPaths::DocumentsLocation);
                                    return paths[0] + "/ossia/score";
                                  }()};

static auto list()
{
  return std::tie(RootPath);
}
}

static void initUserLibrary(const QDir& userlib)
{
  userlib.mkpath("./medias");
  userlib.mkpath("./presets");
  userlib.mkpath("./devices");
  userlib.mkpath("./cues");
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);

  for(auto& path : {getPackagesPath(), getUserLibraryPath()})
  {
    QDir{path}.mkpath(".");
  }
  initUserLibrary(getUserLibraryPath());
}

QString Model::getPackagesPath() const noexcept
{
  return m_RootPath + "/packages";
}

QString Model::getDefaultLibraryPath() const noexcept
{
  return m_RootPath + "/packages/default";
}

QString Model::getUserLibraryPath() const noexcept
{
  return m_RootPath + "/packages/user";
}

QString Model::getUserPresetsPath() const noexcept
{
  return m_RootPath + "/packages/user/presets";
}

QString Model::getSDKPath() const noexcept
{
  return m_RootPath + "/sdk";
}

SCORE_SETTINGS_PARAMETER_CPP(QString, Model, RootPath)

Presenter::Presenter(Model& m, View& v, QObject* parent)
    : score::GlobalSettingsPresenter{m, v, parent}
{
  SETTINGS_PRESENTER(RootPath);
}

QString Presenter::settingsName()
{
  return tr("Library");
}

QIcon Presenter::settingsIcon()
{
  return makeIcons(
      QStringLiteral(":/icons/settings_library_on.png"),
      QStringLiteral(":/icons/settings_library_off.png"),
      QStringLiteral(":/icons/settings_library_off.png"));
}

View::View()
{
  m_widg = new score::FormWidget{tr("Library")};

  auto lay = m_widg->layout();

  SETTINGS_UI_PATH_SETUP("Default Path", RootPath);
}

SETTINGS_UI_PATH_IMPL(RootPath)
QWidget* View::getWidget()
{
  return m_widg;
}
}
