#include "ApplicationPlugin.hpp"

#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Audio/AudioDevice.hpp>
#include <Audio/Settings/Model.hpp>
#include <LV2/Context.hpp>
#include <LV2/EffectModel.hpp>
#include <Media/AudioPluginCache.hpp>
#include <Media/Effect/Settings/Model.hpp>
#include <Media/PluginScanner.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/ThreadPool.hpp>

#include <ossia/audio/audio_protocol.hpp>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <wobjectimpl.h>
W_OBJECT_IMPL(LV2::ApplicationPlugin)

SCORE_SERALIZE_DATASTREAM_DEFINE(LV2::PluginInfo)
SCORE_SERALIZE_DATASTREAM_DEFINE(std::vector<LV2::PluginInfo>)

template <>
void DataStreamReader::read(const LV2::PluginInfo& p)
{
  m_stream << p.bundle << p.uri << p.name << p.class_label << p.author
           << p.documentationLink << p.mtime << p.size << p.audio_in << p.audio_out
           << p.midi_in << p.midi_out << p.control_in << p.control_out << p.atom_in
           << p.atom_out << p.cv_in << p.has_ui << p.ui_uris << p.preset_uris
           << p.preset_bundles << p.valid;
}
template <>
void DataStreamWriter::write(LV2::PluginInfo& p)
{
  m_stream >> p.bundle >> p.uri >> p.name >> p.class_label >> p.author
      >> p.documentationLink >> p.mtime >> p.size >> p.audio_in >> p.audio_out
      >> p.midi_in >> p.midi_out >> p.control_in >> p.control_out >> p.atom_in
      >> p.atom_out >> p.cv_in >> p.has_ui >> p.ui_uris >> p.preset_uris
      >> p.preset_bundles >> p.valid;
}

namespace LV2
{
void on_uiMessage(
    SuilController controller, uint32_t port_index, uint32_t buffer_size,
    uint32_t protocol, const void* buffer)
{
  auto& fx = *(Model*)controller;
  if(protocol == 0)
  {
    auto it = fx.control_map.find(port_index);
    if(it != fx.control_map.end())
    {
      // currently writing from score
      if(it->second.second)
        return;
    }
  }

  Message c{port_index, protocol, {}};
  c.body.resize(buffer_size);
  auto b = (const uint8_t*)buffer;
  for(uint32_t i = 0; i < buffer_size; i++)
    c.body[i] = b[i];

  fx.ui_events.enqueue(std::move(c));
}

uint32_t port_index(SuilController controller, const char* symbol)
{
  // Seen with cardinal, causes a crash in lilv_plugin_get_port_by_symbol
  if(symbol == std::string_view{"lv2_enabled"})
    return LV2UI_INVALID_PORT_INDEX;

  auto& p = score::GUIAppContext().applicationPlugin<LV2::ApplicationPlugin>();
  Model& fx = (Model&)controller;
  auto n = lilv_new_uri(p.lilv.me, symbol);
  auto port = lilv_plugin_get_port_by_symbol(fx.plugin, n);
  lilv_node_free(n);
  return port ? lilv_port_get_index(fx.plugin, port) : LV2UI_INVALID_PORT_INDEX;
}
}

namespace LV2
{
namespace
{
constexpr quint32 cache_format_version = 1;
const QString cache_key = QStringLiteral("Effect/KnownLV2Cache");
const QString legacy_cache_key = QStringLiteral("Effect/KnownLV2");
}

ApplicationPlugin::ApplicationPlugin(const score::ApplicationContext& app)
    : score::ApplicationPlugin{app}
    , lv2_context{std::make_unique<LV2::GlobalContext>(64, lv2_host_context)}
    , lv2_host_context{lv2_context.get(), nullptr, lv2_context->features(), lilv}
{
  qRegisterMetaType<PluginInfo>();
  qRegisterMetaType<std::vector<PluginInfo>>();

  // rescan rebuilds the lilv index, invalidating cached LilvPlugin* in find_lv2_plugin
  QObject::connect(
      this, &ApplicationPlugin::descriptorsChanged, this,
      [] { LV2::clearPluginCache(); });

  if(!qEnvironmentVariableIsEmpty("SCORE_DISABLE_AUDIOPLUGINS"))
    return;

  if(!qEnvironmentVariableIsEmpty("SCORE_DISABLE_LV2"))
    return;

  lv2_context->loadPlugins();

  lv2_context->ui_host
      = suil.host_new(LV2::on_uiMessage, LV2::port_index, nullptr, nullptr);

  score::TaskPool::instance();
  for(int i = 0; i < 100; i++)
  {
    std::vector<char> v;
    v.reserve(65535);
    lv2_host_context.release_worker_data(std::move(v));
  }

  // Coalesce disk writes + tree rebuilds: one per batch, not one per puppet
  m_persistTimer = new QTimer{this};
  m_persistTimer->setSingleShot(true);
  m_persistTimer->setInterval(500);
  QObject::connect(m_persistTimer, &QTimer::timeout, this, [this] {
    persistCache();
    descriptorsChanged();
  });

#if QT_CONFIG(process)
  m_scanner = std::make_unique<Media::PluginScanner>("lv2-scanner");
  // 60s: lsp-plugins (~197 plug-ins, ASan) can push past 10s
  m_scanner->setProcessTimeout(60000);
  con(*m_scanner, &Media::PluginScanner::scanned, this, &ApplicationPlugin::onScanned);
  con(*m_scanner, &Media::PluginScanner::scanFailed, this,
      [this](const QString& bundle, const QString& reason) {
    qDebug() << "LV2 scan failed for" << bundle << ":" << reason;
    markBundleInvalid(bundle);
      });
  con(*m_scanner, &Media::PluginScanner::done, this, [this] {
    persistCache();
    descriptorsChanged();
  });
#endif
}

void ApplicationPlugin::forceRescan()
{
  m_plugins.clear();
  rescanBundles();
}

void ApplicationPlugin::initialize()
{
  lv2_context->sampleRate = context.settings<Audio::Settings::Model>().getRate();

  if(!qEnvironmentVariableIsEmpty("SCORE_DISABLE_AUDIOPLUGINS"))
    return;
  if(!qEnvironmentVariableIsEmpty("SCORE_DISABLE_LV2"))
    return;

  // Load, and heal caches damaged by the pre-token protocol (duplicated
  // entries, valid/invalid pairs for the same bundle)
  m_plugins = Media::loadPluginCache<PluginInfo>(
      cache_format_version, cache_key, legacy_cache_key);
  Media::sanitizePluginCache(
      m_plugins,
      // -> QString: same QStringBuilder dangling-temporary trap as the CLAP key.
      [](const PluginInfo& i) -> QString { return i.bundle + "|" + i.uri; },
      [](const PluginInfo& i) { return i.bundle; },
      [](const PluginInfo& i) { return i.valid; });
  // Make the migration/healing durable even if no scan runs this session
  persistCache();

  descriptorsChanged();

  auto& set = context.settings<Media::Settings::Model>();
  con(set, &Media::Settings::Model::Lv2PathsChanged, this,
      [this] { rescanBundles(); });

  rescanBundles();
}

ApplicationPlugin::~ApplicationPlugin()
{
#if QT_CONFIG(process)
  // Reaps any puppet still running
  m_scanner.reset();
#endif

  // Results delivered so far would otherwise be lost when quitting mid-scan:
  // the debounced m_persistTimer dies with us. Only when a scan actually ran —
  // in disabled mode m_plugins is empty and would wipe the on-disk cache.
  if(m_scanRan)
    persistCache();

  // The plug_map memoizes LilvPlugin* into our world; another
  // ApplicationPlugin in this process (tests) must not see stale ones
  LV2::clearPluginCache();

  suil.host_free(lv2_context->ui_host);
}

const PluginInfo*
ApplicationPlugin::findDescriptor(const QString& uri_or_bundle) const noexcept
{
  // Empty-bundle markers can be valid-with-empty-name; never match on ""
  if(uri_or_bundle.isEmpty())
    return nullptr;

  for(const auto& p : m_plugins)
  {
    if(p.valid && p.uri == uri_or_bundle)
      return &p;
  }
  QString path = uri_or_bundle;
  if(path.startsWith("file://"))
    path = QUrl(path).toLocalFile();
  while(path.size() > 1 && path.endsWith('/'))
    path.chop(1);

  for(const auto& p : m_plugins)
  {
    if(!p.valid)
      continue;
    QString bp = p.bundle;
    while(bp.size() > 1 && bp.endsWith('/'))
      bp.chop(1);
    if(bp == path)
      return &p;
  }

  // Documents saved before the URI-based chooser reference plugins by display
  // name. Names are not unique across bundles: first valid match wins, same as
  // the pre-scanner lookup did against the lilv world.
  for(const auto& p : m_plugins)
  {
    if(p.valid && !p.name.isEmpty() && p.name == uri_or_bundle)
      return &p;
  }
  return nullptr;
}

void ApplicationPlugin::setCachedDescriptors(std::vector<PluginInfo> descriptors)
{
  m_plugins = std::move(descriptors);
  descriptorsChanged();
}

void ApplicationPlugin::ensureBundleLoaded(const QString& uri_or_bundle)
{
  QString bundle;
  if(auto* info = findDescriptor(uri_or_bundle))
  {
    bundle = info->bundle;
  }
  else if(uri_or_bundle.startsWith("file://"))
  {
    bundle = uri_or_bundle;
  }
  else
  {
    return;
  }

  if(bundle.isEmpty())
    return;

  // QUrl::fromLocalFile handles RFC 8089 cross-platform (Windows drive path third slash)
  QString bundle_uri;
  if(bundle.startsWith("file://"))
  {
    bundle_uri = bundle;
  }
  else
  {
    QString path = bundle;
    while(path.size() > 1 && path.endsWith('/'))
      path.chop(1);
    bundle_uri = QUrl::fromLocalFile(path).toString();
  }
  if(!bundle_uri.endsWith('/'))
    bundle_uri.append('/');

  if(m_loaded_bundles.contains(bundle_uri))
    return;

  LilvNode* node = lilv_new_uri(lilv.me, bundle_uri.toUtf8().constData());
  if(!node)
    return;

  lilv_world_load_bundle(lilv.me, node);
  lilv_node_free(node);

  // Re-run specs + classes so the new bundle's class hierarchy resolves (idempotent)
  lilv_world_load_specifications(lilv.me);
  lilv_world_load_plugin_classes(lilv.me);

  m_loaded_bundles.insert(bundle_uri);
}

QStringList ApplicationPlugin::discoverLv2Search()
{
  // Configured search paths; defaults live in Media::Settings::Model
  QStringList paths = context.settings<Media::Settings::Model>().getLv2Paths();

  if(qEnvironmentVariableIsSet("LV2_PATH"))
  {
    auto extra = QString::fromUtf8(qgetenv("LV2_PATH"))
                     .split(QDir::listSeparator(), Qt::SkipEmptyParts);
    paths += extra;
  }

  for(auto it = paths.begin(); it != paths.end();)
  {
    auto fi = QFileInfo{*it};
    if(!fi.exists() || !fi.isDir())
    {
      it = paths.erase(it);
    }
    else
    {
      *it = fi.canonicalFilePath();
      ++it;
    }
  }
  paths.removeDuplicates();
  return paths;
}

// Cache key over all *.ttl in the bundle: manifest-only mtime missed sibling TTL edits
static std::pair<qint64, qint64> bundleCacheKey(const QString& bundlePath)
{
  qint64 max_mtime = 0;
  qint64 total_size = 0;
  QDirIterator it(
      bundlePath, QStringList{QStringLiteral("*.ttl")}, QDir::Files,
      QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
  while(it.hasNext())
  {
    it.next();
    const QFileInfo fi = it.fileInfo();
    const qint64 m = fi.lastModified().toMSecsSinceEpoch();
    if(m > max_mtime)
      max_mtime = m;
    total_size += fi.size();
  }
  if(max_mtime == 0)
  {
    QFileInfo manifest{bundlePath + QStringLiteral("/manifest.ttl")};
    max_mtime = manifest.lastModified().toMSecsSinceEpoch();
    total_size = manifest.size();
  }
  return {max_mtime, total_size};
}

QStringList ApplicationPlugin::discoverBundles(const QStringList& search)
{
  QStringList bundles;
  for(const QString& dir : search)
  {
    if(!QDir{dir}.isReadable())
      continue;
    QDirIterator it(
        dir, QStringList{"*.lv2"}, QDir::Dirs | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while(it.hasNext())
    {
      QString bundlePath = it.next();
      if(!QFile::exists(bundlePath + "/manifest.ttl"))
        continue;
      bundles.push_back(QFileInfo{bundlePath}.canonicalFilePath());
    }
  }
  bundles.removeDuplicates();
  return bundles;
}

QStringList ApplicationPlugin::discoverSpecBundles(const QStringList& bundles)
{
  QStringList specs;
  for(const QString& b : bundles)
  {
    QFile manifest(b + QStringLiteral("/manifest.ttl"));
    if(!manifest.open(QIODevice::ReadOnly))
      continue;
    // Substring match: spec manifests are tiny, no need for a TTL parser here
    const QByteArray content = manifest.readAll();
    if(content.contains("lv2:Specification")
       || content.contains("<http://lv2plug.in/ns/lv2core#Specification>"))
    {
      specs.push_back(b);
    }
  }
  return specs;
}

static const QString& lv2puppetPath();

void ApplicationPlugin::rescanBundles()
{
#if QT_CONFIG(process)
  // Not created when LV2 support is disabled (settings UI can still call us)
  if(!m_scanner)
    return;

  const auto search = discoverLv2Search();
  const auto bundles = discoverBundles(search);

  // Pass specs to every puppet so a malformed third-party bundle can't crash a scan
  m_spec_bundles = discoverSpecBundles(bundles);

  ossia::hash_map<QString, std::pair<qint64, qint64>> cache_index;
  cache_index.reserve(m_plugins.size());
  for(const auto& p : m_plugins)
  {
    if(p.valid)
      cache_index[p.bundle] = {p.mtime, p.size};
  }

  QStringList toScan;
  for(const QString& bundle : bundles)
  {
    const auto [mtime, size] = bundleCacheKey(bundle);
    auto it = cache_index.find(bundle);
    if(it == cache_index.end())
    {
      toScan.push_back(bundle);
    }
    else if(it->second.first != mtime || it->second.second != size)
    {
      for(int i = m_plugins.size() - 1; i >= 0; --i)
      {
        if(m_plugins[i].bundle == bundle)
          m_plugins.erase(m_plugins.begin() + i);
      }
      toScan.push_back(bundle);
    }
  }

  {
    ossia::hash_set<QString> on_disk;
    on_disk.reserve(bundles.size());
    for(const auto& b : bundles)
      on_disk.insert(b);
    for(int i = m_plugins.size() - 1; i >= 0; --i)
    {
      if(!on_disk.contains(m_plugins[i].bundle))
        m_plugins.erase(m_plugins.begin() + i);
    }
  }

  if(!toScan.isEmpty())
  {
    descriptorsChanged(); // stale cache entries may have been removed
  }

  if(toScan.isEmpty())
  {
    persistCache();
    descriptorsChanged();
    return;
  }

  m_scanRan = true;
  m_scanner->setPuppet(lv2puppetPath());
  m_scanner->setEnvironmentProvider([specs = m_spec_bundles] {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Disable LSan in the child: lilv leaks on shutdown; we'd see QProcess::Crashed
    env.insert(
        "ASAN_OPTIONS", env.value("ASAN_OPTIONS", "") + ":detect_leaks=0:exitcode=0");
    env.insert("LSAN_OPTIONS", env.value("LSAN_OPTIONS", "") + ":exitcode=0");
    if(!specs.isEmpty())
      env.insert("LV2PUPPET_SPECS", specs.join(QDir::listSeparator()));
    return env;
  });
  m_scanner->scan(toScan);
#endif
}

static const QString& lv2puppetPath()
{
  static const QString path = []() -> QString {
    auto app = QCoreApplication::instance()->applicationDirPath();
#if defined(__APPLE__)
    auto bundle_path
        = "/ossia-score-lv2puppet.app/Contents/MacOS/"
          "ossia-score-lv2puppet";
    QString bundle_puppet = app + bundle_path;
    if(QFile::exists(bundle_puppet))
      return bundle_puppet;
    else if(QFile::exists(app + "/ossia-score-lv2puppet"))
      return QString(app + "/ossia-score-lv2puppet");
    else if(QFile::exists(app + "/../../ossia-score-lv2puppet"))
      return QString(app + "/../../ossia-score-lv2puppet");
    else if(QFile::exists(app + "/../../" + bundle_path))
      return QString(app + "/../../" + bundle_path);
    else
      return QStringLiteral("ossia-score-lv2puppet");
#else
    return app + "/ossia-score-lv2puppet";
#endif
  }();
  return path;
}

void ApplicationPlugin::onScanned(const QString& bundlePath, const QJsonObject& obj)
{
  // The reply's own "Bundle" field is informative only; the scan session
  // knows which bundle this reply belongs to.
  if(obj.contains("Plugins") && obj["Plugins"].isArray())
    addPluginsFromJson(bundlePath, obj["Plugins"].toArray());
  else
    markBundleInvalid(bundlePath);
}

void ApplicationPlugin::addPluginsFromJson(
    const QString& bundlePath, const QJsonArray& arr)
{
  const auto [mtime, size] = bundleCacheKey(bundlePath);

  for(int i = m_plugins.size() - 1; i >= 0; --i)
  {
    if(m_plugins[i].bundle == bundlePath)
      m_plugins.erase(m_plugins.begin() + i);
  }

  for(const auto& v : arr)
  {
    if(!v.isObject())
      continue;
    const auto o = v.toObject();
    PluginInfo info;
    info.bundle = bundlePath;
    info.uri = o["uri"].toString();
    info.name = o["name"].toString();
    info.class_label = o["class"].toString();
    info.author = o["author"].toString();
    info.documentationLink = o["documentationLink"].toString();
    info.mtime = mtime;
    info.size = size;
    info.audio_in = o["audio_in"].toInt();
    info.audio_out = o["audio_out"].toInt();
    info.midi_in = o["midi_in"].toInt();
    info.midi_out = o["midi_out"].toInt();
    info.control_in = o["control_in"].toInt();
    info.control_out = o["control_out"].toInt();
    info.atom_in = o["atom_in"].toInt();
    info.atom_out = o["atom_out"].toInt();
    info.cv_in = o["cv_in"].toInt();
    info.has_ui = o["has_ui"].toBool();
    if(o["ui_uris"].isArray())
    {
      for(const auto& u : o["ui_uris"].toArray())
        info.ui_uris.push_back(u.toString());
    }
    if(o["preset_uris"].isArray())
    {
      for(const auto& u : o["preset_uris"].toArray())
        info.preset_uris.push_back(u.toString());
    }
    if(o["preset_bundles"].isArray())
    {
      for(const auto& u : o["preset_bundles"].toArray())
        info.preset_bundles.push_back(u.toString());
    }
    info.valid = !info.uri.isEmpty();
    m_plugins.push_back(std::move(info));
  }

  // Empty-bundle marker: matches mtime+size so the next rescan is a cache hit
  if(arr.isEmpty())
  {
    PluginInfo marker;
    marker.bundle = bundlePath;
    marker.name = "<Empty>";
    marker.mtime = mtime;
    marker.size = size;
    marker.valid = false;
    m_plugins.push_back(std::move(marker));
  }

  // Don't restart a running timer: a busy scan would starve the debounce
  if(m_persistTimer && !m_persistTimer->isActive())
    m_persistTimer->start();
}

void ApplicationPlugin::markBundleInvalid(const QString& bundlePath)
{
  // Marker so we don't re-scan the broken bundle every run; mtime/size for fix detection
  const auto [mtime, size] = bundleCacheKey(bundlePath);
  PluginInfo info;
  info.bundle = bundlePath;
  info.name = "<Invalid>";
  info.mtime = mtime;
  info.size = size;
  info.valid = false;

  for(int i = m_plugins.size() - 1; i >= 0; --i)
  {
    if(m_plugins[i].bundle == bundlePath)
      m_plugins.erase(m_plugins.begin() + i);
  }
  m_plugins.push_back(std::move(info));

  // Don't restart a running timer: a busy scan would starve the debounce
  if(m_persistTimer && !m_persistTimer->isActive())
    m_persistTimer->start();
}

void ApplicationPlugin::persistCache()
{
  Media::savePluginCache(cache_format_version, cache_key, m_plugins);
}

}
