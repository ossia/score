#include <Library/LibrarySettings.hpp>

#include <score/command/Command.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/SettingsCommand.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/FormWidget.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFormLayout>
#include <QLineEdit>
#include <QSet>
#include <QStandardPaths>

#include <wobjectimpl.h>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <dlfcn.h>
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

// Make the native libraries shipped by "support" packages (in {RootPath}/support)
// loadable by addons that dlopen / LoadLibrary them by bare name.
static void addSupportLibraries(const QDir& supportDir)
{
  if(!supportDir.exists())
    return;

#ifdef Q_OS_WIN
  // On Windows we can extend the DLL search path: register every directory that
  // contains a .dll, so a later LoadLibrary("foo.dll") from an addon resolves.
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
#else
  // On Linux/macOS there is no runtime equivalent of AddDllDirectory: the dynamic
  // loader snapshots LD_LIBRARY_PATH / DYLD_* at process start, so changing them
  // here would not affect later dlopen() calls. Instead, eagerly dlopen every
  // support library with RTLD_GLOBAL: a subsequent bare-name dlopen() from an addon
  // then resolves to the already-loaded object (the loader matches it by soname /
  // install name). We iterate to a fixpoint so that inter-dependent support
  // libraries load regardless of their on-disk order (each successful load makes its
  // symbols / NEEDED entries available to the others). Libraries that never load
  // (e.g. a missing external dependency) are simply skipped — the addon relying on
  // them just stays unavailable, exactly as before.
#if defined(Q_OS_MACOS)
  const QStringList patterns{"*.dylib"};
#else
  const QStringList patterns{"*.so", "*.so.*"};
#endif

  QStringList pending;
  for(QDirIterator it(
          supportDir.absolutePath(), patterns, QDir::Files,
          QDirIterator::Subdirectories);
      it.hasNext();)
    pending.push_back(it.next());

  bool progress = true;
  while(progress && !pending.empty())
  {
    progress = false;
    for(auto it = pending.begin(); it != pending.end();)
    {
      if(dlopen(it->toUtf8().constData(), RTLD_LAZY | RTLD_GLOBAL))
      {
        it = pending.erase(it);
        progress = true;
      }
      else
      {
        ++it;
      }
    }
  }
#endif
}

Model::Model(
    const UuidKey<score::SettingsDelegateFactory>& k, QSettings& set,
    const score::ApplicationContext& ctx)
    : score::SettingsDelegateModel{k, nullptr}
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

  addSupportLibraries(getSupportPath());
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
