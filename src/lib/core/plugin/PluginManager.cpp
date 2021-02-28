// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score/application/ApplicationComponents.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <core/application/ApplicationRegistrar.hpp>
#include <core/plugin/PluginManager.hpp>

#include <ossia/detail/algorithms.hpp>

#include <boost/concept/usage.hpp>
#include <boost/range/algorithm/find_if.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSettings>
#include <QStandardPaths>

#include <utility>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
extern "C" score::Plugin_QtInterface* plugin_instance();
class DLL
{
public:
  explicit DLL(const char* const so) noexcept
  {
#ifdef _WIN32
    impl = (void*)LoadLibraryA(so);
#else
    impl = dlopen(so, RTLD_LAZY | RTLD_LOCAL | RTLD_NODELETE);
#endif
  }

  DLL(const DLL&) noexcept = delete;
  DLL& operator=(const DLL&) noexcept = delete;
  DLL(DLL&& other)
  {
    impl = other.impl;
    other.impl = nullptr;
  }

  DLL& operator=(DLL&& other) noexcept
  {
    impl = other.impl;
    other.impl = nullptr;
    return *this;
  }

  QString errorString() const
  {
#ifdef _WIN32
    return {};
#else
    return QString::fromUtf8(dlerror());
#endif
  }

  ~DLL()
  {
    if (impl)
    {
#ifdef _WIN32
      FreeLibrary((HMODULE)impl);
#else
      dlclose(impl);
#endif
    }
  }

  template <typename T>
  T symbol(const char* const sym) const noexcept
  {
#ifdef _WIN32
    return (T)GetProcAddress((HMODULE)impl, sym);
#else
    return (T)dlsym(impl, sym);
#endif
  }

  operator bool() const { return bool(impl); }

private:
  void* impl{};
};

namespace score
{
namespace PluginLoader
{
/**
 * @brief pluginsDir
 * @return The folder where score should look for plug-ins, static for now.
 */
QStringList pluginsDir()
{
  QStringList l;
#if defined(_WIN32)
  l << (QCoreApplication::applicationDirPath() + "/plugins");
#elif defined(__linux__)
  l << QCoreApplication::applicationDirPath() + "/plugins"
    << QCoreApplication::applicationDirPath() + "/../lib/score"
    << "/usr/lib/score";
#elif defined(__APPLE__) && defined(__MACH__)
  l << QCoreApplication::applicationDirPath() + "/plugins"
    << QCoreApplication::applicationDirPath() + "../Frameworks/score/plugins";
#endif
  QString pwd = QDir{}.absolutePath() + "/plugins";

  if (pwd != l[0])
    l << pwd;
  return l;
}

QStringList addonsDir()
{
  QStringList l;

#if !defined(SCORE_DEPLOYMENT_BUILD)
  l << "addons";
#endif
  l << QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first()
           + "/score/addons";
  qDebug() << l;
  return l;
}

QStringList pluginsBlacklist()
{
  QSettings s;
  return s.value("PluginSettings/Blacklist", QStringList{}).toStringList();
}
QStringList pluginsWhitelist()
{
  QSettings s;
  return s.value("PluginSettings/Whitelist", QStringList{}).toStringList();
}

static bool isBlacklisted(const QString& str)
{
#if !defined(__EMSCRIPTEN__)
  auto whitelist = pluginsWhitelist();
  auto blacklist = pluginsBlacklist();

  if (!whitelist.isEmpty())
  {
    if (ossia::none_of(whitelist, [&](const QString& wl) { return str.contains(wl); }))
      return true;
  }
  return ossia::any_of(blacklist, [&](const QString& bl) { return str.contains(bl); });
#else
  return false;
#endif
}

std::pair<score::Plugin_QtInterface*, PluginLoadingError>
loadPlugin(const QString& fileName, const std::vector<score::Addon>& availablePlugins)
{
  using namespace score::PluginLoader;
#if !defined(QT_STATIC) && QT_CONFIG(library)

  // Check if it is blacklisted
  if (isBlacklisted(fileName))
  {
    return std::make_pair(nullptr, PluginLoadingError::Blacklisted);
  }

  static std::vector<DLL> plugins;
  DLL ptr{fileName.toUtf8().constData()};

  if (ptr)
  {
    auto score_factory = ptr.symbol<decltype(&plugin_instance)>("plugin_instance");
    if (!score_factory)
    {
      qDebug() << "Warning: plugin" << fileName << "is not a correct score plugin.";

      return std::make_pair(nullptr, PluginLoadingError::NotAPlugin);
    }

    auto score_plugin = score_factory();
    if (!score_plugin)
    {
      qDebug() << "Warning: plugin" << fileName << "is not a correct score plugin.";

      return std::make_pair(nullptr, PluginLoadingError::NotAPlugin);
    }

    // Check if the plugin is not already loaded
    auto plug_it = ossia::find_if(availablePlugins, [&](const score::Addon& obj) {
      return obj.plugin && (obj.plugin->key() == score_plugin->key());
    });

    if (plug_it != availablePlugins.end())
    {
      qDebug() << "Warning: plugin" << fileName << "was already loaded (" << plug_it->path
               << "). Not reloading.";

      return std::make_pair(nullptr, PluginLoadingError::AlreadyLoaded);
    }

    // We can load the plug-in
    plugins.push_back(std::move(ptr));
    return std::make_pair(score_plugin, PluginLoadingError::NoError);
  }
  else
  {
    qDebug() << "Error while loading" << fileName << ": " << ptr.errorString();
    return std::make_pair(nullptr, PluginLoadingError::UnknownError);
  }
#endif

  return std::make_pair(nullptr, PluginLoadingError::UnknownError);
}

void loadPluginsInAllFolders(std::vector<score::Addon>& availablePlugins, QStringList additional)
{
  using namespace score::PluginLoader;

#if !defined(QT_STATIC) && !defined(__EMSCRIPTEN__)
  // Load dynamic plug-ins
  for (const QString& pluginsFolder : pluginsDir() + additional)
  {
    QDir pluginsDir(pluginsFolder);
    for (const QString& fileName : pluginsDir.entryList(QDir::Files))
    {
      auto path = pluginsDir.absoluteFilePath(fileName);
      auto plug = loadPlugin(path, availablePlugins);

      switch (plug.second)
      {
        case PluginLoadingError::NoError:
        {
          score::Addon addon;
          addon.path = path;
          addon.plugin = plug.first;
          addon.key = safe_cast<score::Plugin_QtInterface*>(plug.first)->key();
          addon.corePlugin = true;
          availablePlugins.push_back(std::move(addon));
          break;
        }
        default:
          break;
      }
    }
  }
#endif
}

std::optional<score::Addon> makeAddon(
    const QString& addon_path,
    const QJsonObject& json_addon,
    const std::vector<score::Addon>& availablePlugins)
{
  using namespace score::PluginLoader;
  using Funmap = std::map<QString, std::function<void(QJsonValue)>>;

  score::Addon add;

  const Funmap funmap{
      {addonArchitecture(),
       [&](QJsonValue v) {
         QString path = addon_path + "/" + v.toString();
         auto plug = loadPlugin(path, availablePlugins);

         switch (plug.second)
         {
           case PluginLoadingError::NoError:
           {
             add.path = path;
             add.plugin = plug.first;
             break;
           }
           case PluginLoadingError::Blacklisted:
           {
             add.path = path;
             add.enabled = false;
             break;
           }
           default:
             break;
         }
       }},
      {"name", [&](QJsonValue v) { add.name = v.toString(); }},
      {"version", [&](QJsonValue v) { add.version = v.toString(); }},
      {"url", [&](QJsonValue v) { add.latestVersionAddress = v.toString(); }},
      {"short", [&](QJsonValue v) { add.shortDescription = v.toString(); }},
      {"long", [&](QJsonValue v) { add.longDescription = v.toString(); }},
      {"key", [&](QJsonValue v) { add.key = UuidKey<Plugin>::fromString(v.toString()); }},
      {"small", [&](QJsonValue v) { add.smallImage = QImage{v.toString()}; }},
      {"large", [&](QJsonValue v) { add.largeImage = QImage{v.toString()}; }}};

  for (auto k : json_addon.keys())
  {
    auto fun = funmap.find(k);
    if (fun != funmap.end())
    {
      fun->second(json_addon[k]);
    }
  }

  if (add.name.isEmpty() || add.path.isEmpty() || add.key.impl().is_nil())
    return std::nullopt;

  return add;
}

void loadAddonsInAllFolders(std::vector<score::Addon>& availablePlugins)
{
  using namespace score::PluginLoader;

  // Load dynamic plug-ins
  for (const QString& pluginsFolder : addonsDir())
  {
    QDir pluginsDir{pluginsFolder};
    for (const QString& dirName : pluginsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
      QString folder = pluginsFolder + "/" + dirName;
      QFile addonFile{folder + "/localaddon.json"};

      // First look for a addon.json file
      if (!addonFile.exists())
        continue;
      addonFile.open(QFile::ReadOnly);

      auto addon = makeAddon(
          folder, QJsonDocument::fromJson(addonFile.readAll()).object(), availablePlugins);

      if (addon)
        availablePlugins.push_back(std::move(*addon));
    }
  }
}

}
}
