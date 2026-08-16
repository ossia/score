#include "ApplicationPlugin.hpp"

#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Media/AudioPluginCache.hpp>
#include <Media/Effect/Settings/Model.hpp>
#include <Media/PluginScanner.hpp>
#include <Vst/EffectModel.hpp>
#include <Vst/Loader.hpp>

#include <score/tools/Bind.hpp>

#include <QApplication>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <wobjectimpl.h>
W_OBJECT_IMPL(vst::ApplicationPlugin)

SCORE_SERALIZE_DATASTREAM_DEFINE(vst::VSTInfo)
SCORE_SERALIZE_DATASTREAM_DEFINE(std::vector<vst::VSTInfo>)
template <>
void DataStreamReader::read(const vst::VSTInfo& p)
{
  m_stream << p.path << p.prettyName << p.displayName << p.author << p.uniqueID
           << p.controls << p.isSynth << p.isValid;
}
template <>
void DataStreamWriter::write(vst::VSTInfo& p)
{
  m_stream >> p.path >> p.prettyName >> p.displayName >> p.author >> p.uniqueID
      >> p.controls >> p.isSynth >> p.isValid;
}

Q_DECLARE_METATYPE(vst::VSTInfo)
W_REGISTER_ARGTYPE(vst::VSTInfo)
Q_DECLARE_METATYPE(std::vector<vst::VSTInfo>)
W_REGISTER_ARGTYPE(std::vector<vst::VSTInfo>)

namespace vst
{
namespace
{
constexpr quint32 cache_format_version = 1;
const QString cache_key = QStringLiteral("Effect/KnownVST2Cache");
const QString legacy_cache_key = QStringLiteral("Effect/KnownVST2");
}

VSTInfo parseVstReply(const QString& path, const QJsonObject& obj)
{
  VSTInfo i;
  i.path = path;
  i.uniqueID = obj["UniqueID"].toInt();
  i.isSynth = obj["Synth"].toBool();
  i.author = obj["Author"].toString();
  i.displayName = obj["PrettyName"].toString();
  i.controls = obj["Controls"].toInt();
  i.isValid = true;

  // Only way to get a separation between Kontakt 5 / Kontakt 5 (8
  // out) / Kontakt 5 (16 out),  etc...
  i.prettyName = QFileInfo(path).completeBaseName();
  return i;
}

ApplicationPlugin::ApplicationPlugin(const score::ApplicationContext& app)
    : score::ApplicationPlugin{app}
{
  qRegisterMetaType<VSTInfo>();
  qRegisterMetaType<std::vector<VSTInfo>>();

#if QT_CONFIG(process)
  m_scanner = std::make_unique<Media::PluginScanner>("vst-scanner");
  // 30s: bridged plug-ins (yabridge) may need a cold wineserver start, and
  // up to 4 formats x 8 puppets share the machine during a startup scan
  m_scanner->setProcessTimeout(30000);
  con(*m_scanner, &Media::PluginScanner::scanned, this, &ApplicationPlugin::onScanned);
  con(*m_scanner, &Media::PluginScanner::scanFailed, this,
      &ApplicationPlugin::onScanFailed);
  con(*m_scanner, &Media::PluginScanner::done, this, [this] {
    persistCache();
    vstChanged();
  });
#endif

  // Coalesce disk writes + UI updates: one per batch, not one per puppet
  m_persistTimer = new QTimer{this};
  m_persistTimer->setSingleShot(true);
  m_persistTimer->setInterval(500);
  connect(m_persistTimer, &QTimer::timeout, this, [this] {
    persistCache();
    vstChanged();
  });

  // VST idle update
  startTimer(10, Qt::PreciseTimer);
}

void ApplicationPlugin::initialize()
{
  // Load, and heal caches damaged by the pre-token protocol (duplicated
  // entries, valid/invalid pairs for the same path)
  vst_infos = Media::loadPluginCache<VSTInfo>(
      cache_format_version, cache_key, legacy_cache_key);
  Media::sanitizePluginCache(
      vst_infos, [](const VSTInfo& i) { return i.path; },
      [](const VSTInfo& i) { return i.path; },
      [](const VSTInfo& i) { return i.isValid; });
  // Make the migration/healing durable even if no scan runs this session
  persistCache();

  vstChanged();

  auto& set = context.settings<Media::Settings::Model>();
  con(set, &Media::Settings::Model::VstPathsChanged, this,
      &ApplicationPlugin::rescanVSTs);

  if(qEnvironmentVariableIsEmpty("SCORE_DISABLE_AUDIOPLUGINS"))
    rescanVSTs(set.getVstPaths());
}

void ApplicationPlugin::registerRunningVST(Model* m)
{
  m_runningVSTs.push_back(m);
}

void ApplicationPlugin::unregisterRunningVST(Model* m)
{
  auto it = ossia::find(m_runningVSTs, m);
  if(it != m_runningVSTs.end())
  {
    m_runningVSTs.erase(it);
  }
}

static const QString& vstPuppetPath()
{
  static const QString path = []() -> QString {
    auto app = QCoreApplication::instance()->applicationDirPath();
#if defined(__APPLE__)
    auto bundle_path
        = "/ossia-score-vstpuppet.app/Contents/MacOS/"
          "ossia-score-vstpuppet";
    QString bundle_vstpuppet = app + bundle_path;
    if(QFile::exists(bundle_vstpuppet))
      return bundle_vstpuppet;
    else if(QFile::exists(app + "/ossia-score-vstpuppet"))
      return QString(app + "/ossia-score-vstpuppet");
    else if(QFile::exists(app + "/../../ossia-score-vstpuppet"))
      return QString(app + "/../../ossia-score-vstpuppet");
    else if(QFile::exists(app + "/../../" + bundle_path))
      return QString(app + "/../../" + bundle_path);
    else
      return QStringLiteral("ossia-score-vstpuppet");
#else
    return app + "/ossia-score-vstpuppet";
#endif
  }();
  return path;
}

void ApplicationPlugin::clearVSTs()
{
  vst_infos.clear();
  vstChanged();
}

void ApplicationPlugin::rescanVSTs(QStringList paths)
{
#if QT_CONFIG(process)
  // 0. Handle VST_PATH
  if(QFileInfo vst_env_path{QString(qgetenv("VST_PATH"))}; vst_env_path.isDir())
  {
    paths += vst_env_path.absoluteFilePath();
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

  // 1. List all plug-ins in new paths
  QStringList exploredPaths;
  QSet<QString> newPlugins;
  for(QString dir : paths)
  {
    auto canonical_path = QDir{dir}.canonicalPath();
    if(exploredPaths.contains(canonical_path))
      continue;

    exploredPaths.push_back(canonical_path);

#if defined(__APPLE__)
    {
      QDirIterator it(
          dir, QStringList{"*.vst", "*.component"}, QDir::AllEntries,
          QDirIterator::Subdirectories);

      while(it.hasNext())
        newPlugins.insert(it.next());
    }
    {
      QDirIterator it(
          dir, QStringList{"*.dylib"}, QDir::Files, QDirIterator::Subdirectories);
      while(it.hasNext())
      {
        auto path = it.next();
        if(!path.contains(".vst") && !path.contains(".component"))
          newPlugins.insert(path);
      }
    }
#else
    QDirIterator it(
        dir, QStringList{vst::default_filter}, QDir::Files,
        QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while(it.hasNext())
      newPlugins.insert(it.next());
#endif
  }

  // 2. Remove plug-ins not in these paths, keep the known ones
  for(auto it = vst_infos.begin(); it != vst_infos.end();)
  {
    auto new_it = newPlugins.find(it->path);
    if(new_it != newPlugins.end())
    {
      // plug-in is in both sets, no need to rescan it
      newPlugins.erase(new_it);
      ++it;
    }
    else
    {
      it = vst_infos.erase(it);
    }
  }

  vstChanged();

  // 3. Scan the remaining ones out-of-process
  QStringList toScan;
  for(const QString& path : newPlugins)
  {
    if(path.contains("linvst.so"))
      continue;
    toScan.push_back(path);
  }
  toScan.sort();

  m_scanner->setPuppet(vstPuppetPath());
  m_scanner->scan(std::move(toScan));
#endif
}

void ApplicationPlugin::removeEntriesForPath(const QString& path)
{
  ossia::remove_erase_if(
      vst_infos, [&](const VSTInfo& i) { return i.path == path; });
}

void ApplicationPlugin::onScanned(const QString& path, const QJsonObject& obj)
{
  VSTInfo i = parseVstReply(path, obj);

  removeEntriesForPath(path);
  vst_modules.insert({i.uniqueID, nullptr});
  vst_infos.push_back(std::move(i));

  qDebug() << "Loaded VST " << path << "successfully";
  schedulePersist();
}

void ApplicationPlugin::onScanFailed(const QString& path, const QString& reason)
{
  qDebug() << "VST scan failed for" << path << ":" << reason;

  VSTInfo i;
  i.path = path;
  i.prettyName = "invalid";
  i.uniqueID = -1;
  i.isSynth = false;
  i.isValid = false;

  removeEntriesForPath(path);
  vst_infos.push_back(std::move(i));

  schedulePersist();
}

void ApplicationPlugin::persistCache()
{
  Media::savePluginCache(cache_format_version, cache_key, vst_infos);
}

void ApplicationPlugin::schedulePersist()
{
  m_scanRan = true;
  // Don't restart a running timer: during a busy scan replies arrive more
  // often than the interval and a restarting debounce would never fire,
  // deferring both persistence and UI updates to the very end of the scan
  if(!m_persistTimer->isActive())
    m_persistTimer->start();
}

void ApplicationPlugin::timerEvent(QTimerEvent* event)
{
  for(auto vst : m_runningVSTs)
  {
    if(vst->needIdle.exchange(false, std::memory_order_acquire))
    {
      vst->dispatch(effEditIdle);
    }
  }
}

ApplicationPlugin::~ApplicationPlugin()
{
  // Results delivered so far would otherwise be lost when quitting mid-scan:
  // the debounced m_persistTimer dies with us
  if(m_scanRan)
    persistCache();

  for(auto& e : vst_modules)
  {
    delete e.second;
  }
}

GUIApplicationPlugin::GUIApplicationPlugin(const score::GUIApplicationContext& app)
    : score::GUIApplicationPlugin{app}
{
}

void GUIApplicationPlugin::initialize() { }
}
