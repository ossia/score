#include <Library/LibrarySettings.hpp>

#include <score/command/Command.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/SettingsCommand.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/FormWidget.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <QDir>
#include <QDirIterator>
#include <QFormLayout>
#include <QLineEdit>
#include <QSet>
#include <QStandardPaths>

#include <wobjectimpl.h>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
W_OBJECT_IMPL(Library::Settings::View)
W_OBJECT_IMPL(Library::Settings::Model)

namespace Library::Settings
{

namespace Parameters
{
SETTINGS_PARAMETER_IMPL(RootPath){QStringLiteral("Library/RootPath"), ""};

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

static void addSupportDllDirectories(const QDir& supportDir)
{
#ifdef Q_OS_WIN
  if(!supportDir.exists())
    return;

  QSet<QString> added;

  QDirIterator it(
      supportDir.absolutePath(), {"*.dll"}, QDir::Files, QDirIterator::Subdirectories);

  while(it.hasNext())
  {
    it.next();
    QString dir = it.fileInfo().absolutePath();

    if(added.contains(dir))
      continue;
    added.insert(dir);

    AddDllDirectory(reinterpret_cast<PCWSTR>(dir.utf16()));
  }
#endif
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
  if(m_RootPath.isEmpty())
  {
    // Note : not done in the SETTINGS_PARAMETER_IMPL as it does not work well due to
    // being init before main()
    auto paths = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
    QString default_root = QString("%1/%2/%3")
                               .arg(
                                   paths[0], QCoreApplication::organizationName(),
                                   QCoreApplication::applicationName());

    setRootPath(default_root);
  }

  for(auto& path : {getPackagesPath(), getUserLibraryPath()})
  {
    QDir{path}.mkpath(".");
  }
  initUserLibrary(getUserLibraryPath());

  addSupportDllDirectories(getSupportPath());
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

QString Model::getSupportPath() const noexcept
{
  return m_RootPath + "/support";
}

QStringList Model::getIncludePaths() const noexcept
{
  QStringList ret;

  // Add everything in the current project folder
  // FIXME!

  // Add everything in the user lib
  {
    QString defaultLibPath = getUserLibraryPath() + QDir::separator() + "scripts"
                             + QDir::separator() + "include";
    if(QFileInfo fi(defaultLibPath); fi.isDir())
    {
      ret.push_back(fi.canonicalFilePath());
    }
  }

  // Add everything in the custom packages
  {
    QDirIterator dir{getPackagesPath(), QDirIterator::FollowSymlinks};

    while(dir.hasNext())
    {
      dir.next();
      if(dir.fileInfo().isDir())
      {
        QDir d{dir.filePath() + QDir::separator() + "library"};
        if(d.exists() && !d.isEmpty())
        {
          ret.push_back(d.canonicalPath());
        }
      }
    }
  }

  // Add everything in the default package
  {
    QString defaultLibPath = getDefaultLibraryPath() + QDir::separator() + "Scripts"
                             + QDir::separator() + "include";
    if(QFileInfo fi(defaultLibPath); fi.isDir())
    {
      ret.push_back(fi.canonicalFilePath());
    }
  }

  // Custom helper env var if needed
  if(auto paths = qEnvironmentVariable("SCORE_ADDITIONAL_SCRIPT_INCLUDE_PATHS");
     !paths.isEmpty())
  {
    auto p = paths.split(QDir::listSeparator());
    p.removeAll("");
    for(auto& path : p)
    {
      ret.push_back(std::move(path));
    }
  }

  return ret;
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
