#include "ApplicationPlugin.hpp"

#include <Media/AudioPluginCache.hpp>
#include <Media/Effect/Settings/Model.hpp>
#include <Media/PluginScanner.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include <clap/all.h>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Clap::ApplicationPlugin)

SCORE_SERALIZE_DATASTREAM_DEFINE(Clap::PluginInfo)
SCORE_SERALIZE_DATASTREAM_DEFINE(std::vector<Clap::PluginInfo>)

template <>
void DataStreamReader::read(const Clap::PluginInfo& p)
{
  m_stream << p.path << p.id << p.name << p.vendor << p.version << p.url << p.manual_url
           << p.support_url << p.description << p.features << p.valid;
}
template <>
void DataStreamWriter::write(Clap::PluginInfo& p)
{
  m_stream >> p.path >> p.id >> p.name >> p.vendor >> p.version >> p.url >> p.manual_url
      >> p.support_url >> p.description >> p.features >> p.valid;
}
namespace Clap
{
namespace
{
constexpr quint32 cache_format_version = 1;
const QString cache_key = QStringLiteral("Effect/KnownCLAPCache");
const QString legacy_cache_key = QStringLiteral("Effect/KnownCLAP");

QStringList clapSearchPaths(const score::ApplicationContext& ctx)
{
  QStringList paths = ctx.settings<Media::Settings::Model>().getClapPaths();

  // $CLAP_PATH (per CLAP entry.h), supplementing the configured locations.
  if(qEnvironmentVariableIsSet("CLAP_PATH"))
  {
    const auto extra = qEnvironmentVariable("CLAP_PATH")
                           .split(QDir::listSeparator(), Qt::SkipEmptyParts);
    paths.append(extra);
  }

  for(auto it = paths.begin(); it != paths.end();)
  {
    auto& path = *it;
    auto fi = QFileInfo{path};
    if(!fi.exists())
    {
      it = paths.erase(it);
    }
    else
    {
      path = QFileInfo{path}.canonicalFilePath();
      ++it;
    }
  }
  paths.removeDuplicates();
  return paths;
}
}

QString resolveClapEntry(const QString& path)
{
  const QFileInfo fi{path};
  if(fi.isDir())
  {
    // macOS bundle: the binary lives in Contents/MacOS
    const QString inner = path + QString{"/Contents/MacOS/%1"}.arg(fi.baseName());
    if(QFileInfo::exists(inner))
      return inner;
    // Linux bundle-style directory (Cardinal & other DPF plug-ins): the
    // inner *.clap files are found by the recursive iteration themselves
    return {};
  }
  return path;
}

std::vector<PluginInfo> parseClapReply(const QString& path, const QJsonObject& obj)
{
  std::vector<PluginInfo> res;

  if(!obj.contains("Plugins") || !obj["Plugins"].isArray())
    return res;

  for(const auto& plugin : obj["Plugins"].toArray())
  {
    if(!plugin.isObject())
      continue;
    const auto o = plugin.toObject();

    PluginInfo info;
    info.path = path;
    info.id = o["ID"].toString();
    info.name = o["Name"].toString();
    info.vendor = o["Vendor"].toString();
    info.version = o["Version"].toString();
    info.url = o["URL"].toString();
    info.manual_url = o["ManualURL"].toString();
    info.support_url = o["SupportURL"].toString();
    info.description = o["Description"].toString();

    if(o.contains("Features") && o["Features"].isArray())
    {
      for(const auto& feature : o["Features"].toArray())
      {
        if(feature.isString())
          info.features.push_back(feature.toString());
      }
    }
    info.valid = true;
    res.push_back(std::move(info));
  }
  return res;
}

static const QString& clapPuppetPath()
{
  static const QString path = []() -> QString {
    auto app = QCoreApplication::instance()->applicationDirPath();
#if defined(__APPLE__)
    auto bundle_path
        = "/ossia-score-clappuppet.app/Contents/MacOS/"
          "ossia-score-clappuppet";
    QString bundle_puppet = app + bundle_path;
    if(QFile::exists(bundle_puppet))
      return bundle_puppet;
    else if(QFile::exists(app + "/ossia-score-clappuppet"))
      return QString(app + "/ossia-score-clappuppet");
    else if(QFile::exists(app + "/../../ossia-score-clappuppet"))
      return QString(app + "/../../ossia-score-clappuppet");
    else if(QFile::exists(app + "/../../" + bundle_path))
      return QString(app + "/../../" + bundle_path);
    else
      return QStringLiteral("ossia-score-clappuppet");
#else
    return app + "/ossia-score-clappuppet";
#endif
  }();
  return path;
}

ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& app)
    : score::GUIApplicationPlugin{app}
{
  qRegisterMetaType<PluginInfo>();
  qRegisterMetaType<std::vector<PluginInfo>>();

#if QT_CONFIG(process)
  m_scanner = std::make_unique<Media::PluginScanner>("clap-scanner");
  con(*m_scanner, &Media::PluginScanner::scanned, this, &ApplicationPlugin::onScanned);
  con(*m_scanner, &Media::PluginScanner::scanFailed, this,
      &ApplicationPlugin::onScanFailed);
  con(*m_scanner, &Media::PluginScanner::done, this, [this] {
    persistCache();
    pluginsChanged();
  });
#endif

  // Coalesce disk writes + UI updates: one per batch, not one per puppet
  m_persistTimer = new QTimer{this};
  m_persistTimer->setSingleShot(true);
  m_persistTimer->setInterval(500);
  connect(m_persistTimer, &QTimer::timeout, this, [this] {
    persistCache();
    pluginsChanged();
  });
}

ApplicationPlugin::~ApplicationPlugin() = default;

void ApplicationPlugin::initialize()
{
  // Load, and heal caches damaged by the pre-token protocol: replies from
  // other score instances used to be appended and persisted on every run
  // (observed: every plug-in duplicated 200+ times)
  m_plugins = Media::loadPluginCache<PluginInfo>(
      cache_format_version, cache_key, legacy_cache_key);
  Media::sanitizePluginCache(
      m_plugins, [](const PluginInfo& i) { return i.path + "|" + i.id; },
      [](const PluginInfo& i) { return i.path; },
      [](const PluginInfo& i) { return i.valid; });
  // Make the migration/healing durable even if no scan runs this session
  persistCache();

  pluginsChanged();

  auto& set = context.settings<Media::Settings::Model>();
  con(set, &Media::Settings::Model::ClapPathsChanged, this,
      [this] { rescanPlugins(); });

  if(qEnvironmentVariableIsEmpty("SCORE_DISABLE_AUDIOPLUGINS"))
    rescanPlugins();
}

void ApplicationPlugin::forceRescan()
{
  m_plugins.clear();
  rescanPlugins();
}

void ApplicationPlugin::rescanPlugins()
{
#if QT_CONFIG(process)
  const QStringList paths = clapSearchPaths(context);

  // 1. Discover the .clap files on disk
  QSet<QString> onDisk;
  for(const QString& searchPath : paths)
  {
    if(!QDir{searchPath}.isReadable())
      continue;

    QDirIterator it(
        searchPath, QStringList{"*.clap"}, QDir::Files | QDir::Dirs,
        QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while(it.hasNext())
    {
      if(QString entry = resolveClapEntry(it.next()); !entry.isEmpty())
        onDisk.insert(entry);
    }
  }

  // 2. Drop cache entries for plug-ins removed from disk, keep the known
  // ones (the CLAP cache historically never pruned anything)
  ossia::hash_set<QString> known_plugins_paths;
  for(auto it = m_plugins.begin(); it != m_plugins.end();)
  {
    if(onDisk.contains(it->path))
    {
      known_plugins_paths.insert(it->path);
      ++it;
    }
    else
    {
      it = m_plugins.erase(it);
    }
  }

  pluginsChanged();

  // 3. Scan the new ones out-of-process
  QStringList toScan;
  for(const QString& path : onDisk)
    if(!known_plugins_paths.contains(path))
      toScan.push_back(path);
  toScan.sort();

  m_scanner->setPuppet(clapPuppetPath());
  m_scanner->scan(std::move(toScan));
#endif
}

void ApplicationPlugin::removeEntriesForPath(const QString& path)
{
  ossia::remove_erase_if(
      m_plugins, [&](const PluginInfo& i) { return i.path == path; });
}

void ApplicationPlugin::onScanned(const QString& path, const QJsonObject& obj)
{
  auto infos = parseClapReply(path, obj);
  if(infos.empty())
  {
    onScanFailed(path, QStringLiteral("no plug-ins in file"));
    return;
  }

  removeEntriesForPath(path);
  for(auto& info : infos)
    m_plugins.push_back(std::move(info));

  schedulePersist();
}

void ApplicationPlugin::onScanFailed(const QString& path, const QString& reason)
{
  qDebug() << "CLAP scan failed for" << path << ":" << reason;

  PluginInfo info;
  info.path = path;
  info.name = "<Invalid>";
  info.valid = false;

  removeEntriesForPath(path);
  m_plugins.push_back(std::move(info));
  schedulePersist();
}

void ApplicationPlugin::persistCache()
{
  Media::savePluginCache(cache_format_version, cache_key, m_plugins);
}

void ApplicationPlugin::schedulePersist()
{
  m_persistTimer->start();
}
}
