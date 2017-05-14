#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <boost/concept/usage.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <core/application/ApplicationRegistrar.hpp>
#include <core/plugin/PluginManager.hpp>
#include <iscore/application/ApplicationComponents.hpp>
#include <iscore/application/GUIApplicationContext.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <QJsonDocument>
#include <QPluginLoader>
#include <QSettings>
#include <QVariant>
#include <utility>

#include <ossia/detail/algorithms.hpp>
#include <QStandardPaths>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>

namespace iscore
{
namespace PluginLoader
{
/**
 * @brief pluginsDir
 * @return The folder where i-score should look for plug-ins, static for now.
 */
QStringList pluginsDir()
{
  QStringList l;
#if defined(_WIN32)
  l << (QCoreApplication::applicationDirPath() + "/plugins");
#elif defined(__linux__)
  l << QCoreApplication::applicationDirPath() + "/plugins"
    << QCoreApplication::applicationDirPath() + "/../lib/i-score"
    << "/usr/lib/i-score";
#elif defined(__APPLE__) && defined(__MACH__)
  l << QCoreApplication::applicationDirPath() + "/plugins"
    << QCoreApplication::applicationDirPath()
           + "../Frameworks/i-score/plugins";
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

#if !defined(ISCORE_DEPLOYMENT_BUILD)
  l << "addons";
#endif
  l << QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)
               .first()
           + "/i-score/addons";
  qDebug() << l;
  return l;
}

std::pair<iscore::Plugin_QtInterface*, PluginLoadingError> loadPlugin(
    const QString& fileName,
    const std::vector<iscore::Addon>& availablePlugins)
{
  using namespace iscore::PluginLoader;
#if !defined(ISCORE_STATIC_QT)
  auto blacklist = pluginsBlacklist();

  // Check if it is blacklisted
  if (blacklist.contains(fileName))
  {
    return std::make_pair(nullptr, PluginLoadingError::Blacklisted);
  }

#if defined(_MSC_VER)
  QDir iscore_dir = QDir::current();
  QPluginLoader loader{iscore_dir.relativeFilePath(fileName)};
#else
  QPluginLoader loader{fileName};
#endif

  if (QObject* plugin = loader.instance())
  {
    auto iscore_plugin = dynamic_cast<iscore::Plugin_QtInterface*>(plugin);
    if (!iscore_plugin)
    {
      qDebug() << "Warning: plugin" << plugin->metaObject()->className()
               << "is not an i-score plug-in.";
      return std::make_pair(nullptr, PluginLoadingError::NotAPlugin);
    }

    // Check if the plugin is not already loaded
    auto plug_it
        = ossia::find_if(availablePlugins, [&](const iscore::Addon& obj) {
            return obj.plugin && (obj.plugin->key() == iscore_plugin->key());
          });

    if (plug_it != availablePlugins.end())
    {
      qDebug() << "Warning: plugin" << plugin->metaObject()->className()
               << "was already loaded. Not reloading.";

      return std::make_pair(nullptr, PluginLoadingError::AlreadyLoaded);
    }

    // We can load the plug-in
    return std::make_pair(iscore_plugin, PluginLoadingError::NoError);
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
loadPluginsInAllFolders(std::vector<iscore::Addon>& availablePlugins, QStringList additional)
{
  using namespace iscore::PluginLoader;

#if !defined(ISCORE_STATIC_QT)
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
          iscore::Addon addon;
          addon.path = path;
          addon.plugin = plug.first;
          addon.key = safe_cast<iscore::Plugin_QtInterface*>(plug.first)->key();
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

optional<iscore::Addon> makeAddon(
    const QString& addon_path,
    const QJsonObject& json_addon,
    const std::vector<iscore::Addon>& availablePlugins)
{
  using namespace iscore::PluginLoader;
  using Funmap = std::map<QString, std::function<void(QJsonValue)>>;

  iscore::Addon add;

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
loadAddonsInAllFolders(std::vector<iscore::Addon>& availablePlugins)
{
  using namespace iscore::PluginLoader;

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
  QSettings s;
  return s.value("PluginSettings/Blacklist", QStringList{}).toStringList();
}
}
}
