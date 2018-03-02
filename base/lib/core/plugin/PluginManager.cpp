// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <boost/concept/usage.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <core/application/ApplicationRegistrar.hpp>
#include <core/plugin/PluginManager.hpp>
#include <score/application/ApplicationComponents.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <QJsonDocument>
#include <QPluginLoader>
#include <QSettings>
#include <QVariant>
#include <utility>

#include <ossia/detail/algorithms.hpp>
#include <QStandardPaths>
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>

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
    << QCoreApplication::applicationDirPath()
           + "../Frameworks/score/plugins";
#endif
  auto pwd = QDir{}.absolutePath() + "/plugins";

  if(pwd != l[0])
    l << pwd;
  qDebug() << l;
  return l;
}

QStringList addonsDir()
{
  QStringList l;

#if !defined(SCORE_DEPLOYMENT_BUILD)
  l << "addons";
#endif
  l << QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)
               .first()
           + "/score/addons";
  qDebug() << l;
  return l;
}

std::pair<score::Plugin_QtInterface*, PluginLoadingError> loadPlugin(
    const QString& fileName,
    const std::vector<score::Addon>& availablePlugins)
{
  using namespace score::PluginLoader;
#if !defined(SCORE_STATIC_QT) && QT_CONFIG(library)
  auto blacklist = pluginsBlacklist();

  // Check if it is blacklisted
  if (blacklist.contains(fileName))
  {
    return std::make_pair(nullptr, PluginLoadingError::Blacklisted);
  }

#if defined(_MSC_VER)
  QDir score_dir = QDir::current();
  qDebug() << "Loading: " << fileName << score_dir.relativeFilePath(fileName);
  QPluginLoader loader{ fileName };//score_dir.relativeFilePath(fileName)};
#else
  QPluginLoader loader{fileName};
#endif

  if (QObject* plugin = loader.instance())
  {
    auto score_plugin = dynamic_cast<score::Plugin_QtInterface*>(plugin);
    if (!score_plugin)
    {
      qDebug() << "Warning: plugin" << plugin->metaObject()->className()
               << "is not an score plug-in.";
      return std::make_pair(nullptr, PluginLoadingError::NotAPlugin);
    }

    // Check if the plugin is not already loaded
    auto plug_it
        = ossia::find_if(availablePlugins, [&](const score::Addon& obj) {
            return obj.plugin && (obj.plugin->key() == score_plugin->key());
          });

    if (plug_it != availablePlugins.end())
    {
      qDebug() << "Warning: plugin" << plugin->metaObject()->className()
               << "was already loaded. Not reloading.";

      return std::make_pair(nullptr, PluginLoadingError::AlreadyLoaded);
    }

    // We can load the plug-in
    return std::make_pair(score_plugin, PluginLoadingError::NoError);
  }
  else
  {
    QString s = loader.errorString();
    if (!s.contains("Plugin verification data mismatch")
        && !s.contains("is not a Qt plugin"))
      qDebug() << "Error while loading" << fileName << ": "
               << loader.errorString();
    return std::make_pair(nullptr, PluginLoadingError::UnknownError);
  }
#endif

  return std::make_pair(nullptr, PluginLoadingError::UnknownError);
}

void
loadPluginsInAllFolders(std::vector<score::Addon>& availablePlugins, QStringList additional)
{
  using namespace score::PluginLoader;

#if !defined(SCORE_STATIC_QT) && !defined(__EMSCRIPTEN__)
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

optional<score::Addon> makeAddon(
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
      {"key",
       [&](QJsonValue v) {
         add.key = UuidKey<Plugin>::fromString(v.toString());
       }},
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
    return ossia::none;

  return add;
}

void
loadAddonsInAllFolders(std::vector<score::Addon>& availablePlugins)
{
  using namespace score::PluginLoader;

  // Load dynamic plug-ins
  for (const QString& pluginsFolder : addonsDir())
  {
    QDir pluginsDir{pluginsFolder};
    for (const QString& dirName :
         pluginsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
      auto folder = pluginsFolder + "/" + dirName;
      QFile addonFile{folder + "/localaddon.json"};

      // First look for a addon.json file
      if (!addonFile.exists())
        continue;
      addonFile.open(QFile::ReadOnly);

      auto addon = makeAddon(
          folder,
          QJsonDocument::fromJson(addonFile.readAll()).object(),
          availablePlugins);

      if (addon)
        availablePlugins.push_back(std::move(*addon));
    }
  }
}

QStringList pluginsBlacklist()
{
  QSettings s("OSSIA", "score");
  return s.value("PluginSettings/Blacklist", QStringList{}).toStringList();
}
}
}
