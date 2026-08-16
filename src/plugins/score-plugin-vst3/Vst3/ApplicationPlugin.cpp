#include <Media/AudioPluginCache.hpp>
#include <Media/Effect/Settings/Model.hpp>
#include <Media/PluginScanner.hpp>
#include <Vst3/ApplicationPlugin.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/math.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include <wobjectimpl.h>

#include <sstream>

W_OBJECT_IMPL(vst3::ApplicationPlugin)

SCORE_SERALIZE_DATASTREAM_DEFINE(vst3::AvailablePlugin)
SCORE_SERALIZE_DATASTREAM_DEFINE(std::vector<vst3::AvailablePlugin>)

Q_DECLARE_METATYPE(vst3::AvailablePlugin)
W_REGISTER_ARGTYPE(vst3::AvailablePlugin)
Q_DECLARE_METATYPE(std::vector<vst3::AvailablePlugin>)
W_REGISTER_ARGTYPE(std::vector<vst3::AvailablePlugin>)

template <>
void DataStreamReader::read(const VST3::Hosting::ClassInfo& pp)
{
  auto& p = const_cast<VST3::Hosting::ClassInfo&>(pp);
  auto& d = p.get();
  m_stream << d.classID.toString() << d.cardinality << d.category << d.name << d.vendor
           << d.version << d.sdkVersion << d.subCategories
           << (const uint32_t&)d.classFlags;
}
template <>
void DataStreamWriter::write(VST3::Hosting::ClassInfo& p)
{
  auto& d = p.get();
  std::string clsid;
  m_stream >> clsid >> d.cardinality >> d.category >> d.name >> d.vendor >> d.version
      >> d.sdkVersion >> d.subCategories >> (uint32_t&)d.classFlags;
  if(auto id = VST3::UID::fromString(clsid))
    d.classID = *id;
  else
    qDebug() << "Invalid VST3 UID:" << clsid.c_str();
}

template <>
void DataStreamReader::read(const vst3::AvailablePlugin& p)
{
  m_stream << p.path << p.name << p.classInfo << p.isValid;
}
template <>
void DataStreamWriter::write(vst3::AvailablePlugin& p)
{
  m_stream >> p.path >> p.name >> p.classInfo >> p.isValid;
}
namespace vst3
{
namespace
{
constexpr quint32 cache_format_version = 1;
const QString cache_key = QStringLiteral("Effect/KnownVST3Cache");
const QString legacy_cache_key = QStringLiteral("Effect/KnownVST3");

static const constexpr auto default_filter = "*.vst3";
#if defined(_WIN32)
static const constexpr auto default_format = QDir::Files;
#else
static const constexpr auto default_format = QDir::Dirs;
#endif
}

VST3::Hosting::ClassInfo::SubCategories
parseSubCategories(const std::string& str) noexcept
{
  std::vector<std::string> vec;
  std::stringstream stream(str);
  std::string item;
  while(std::getline(stream, item, '|'))
    vec.emplace_back(std::move(item));
  return vec;
}

AvailablePlugin parseVst3Reply(const QString& path, const QJsonObject& obj)
{
  AvailablePlugin i;
  i.path = path;
  i.name = obj["Name"].toString();
  i.url = obj["Url"].toString();

  const auto& classes = obj["Classes"].toArray();
  i.classInfo.reserve(classes.size());

  for(const QJsonValue& v : classes)
  {
    const QJsonObject& obj = v.toObject();
    const auto uid = VST3::UID::fromString(obj["UID"].toString().toStdString());
    if(!uid)
      continue;

    i.classInfo.resize(i.classInfo.size() + 1);
    VST3::Hosting::ClassInfo& cls = i.classInfo.back();

    cls.get().classID = *uid;
    cls.get().cardinality = obj["Cardinality"].toInt();
    cls.get().category = obj["Category"].toString().toStdString();
    cls.get().name = obj["Name"].toString().toStdString();
    cls.get().vendor = obj["Vendor"].toString().toStdString();
    cls.get().version = obj["Version"].toString().toStdString();
    cls.get().sdkVersion = obj["SDKVersion"].toString().toStdString();
    cls.get().subCategories
        = parseSubCategories(obj["Subcategories"].toString().toStdString());
    cls.get().classFlags = (uint32_t)obj["ClassFlags"].toDouble();
  }

  i.isValid = !i.classInfo.empty();
  return i;
}

ApplicationPlugin::ApplicationPlugin(const score::ApplicationContext& ctx)
    : score::ApplicationPlugin{ctx}
{
  qRegisterMetaType<AvailablePlugin>();
  qRegisterMetaType<std::vector<AvailablePlugin>>();

#if QT_CONFIG(process)
  m_scanner = std::make_unique<Media::PluginScanner>("vst3-scanner");
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
}

ApplicationPlugin::~ApplicationPlugin()
{
  // Results delivered so far would otherwise be lost when quitting mid-scan:
  // the debounced m_persistTimer dies with us
  if(m_scanRan)
    persistCache();
}

void ApplicationPlugin::initialize()
{
  // Load, and heal caches damaged by the pre-token protocol (duplicated
  // entries, valid/invalid pairs for the same path)
  vst_infos = Media::loadPluginCache<AvailablePlugin>(
      cache_format_version, cache_key, legacy_cache_key);
  Media::sanitizePluginCache(
      vst_infos, [](const AvailablePlugin& i) { return i.path; },
      [](const AvailablePlugin& i) { return i.path; },
      [](const AvailablePlugin& i) { return i.isValid; });
  // Make the migration/healing durable even if no scan runs this session
  persistCache();

  vstChanged();

  // Note: our own path list, not VST2's - editing the VST2 paths used to
  // wipe and rescan the whole VST3 database
  auto& set = context.settings<Media::Settings::Model>();
  con(set, &Media::Settings::Model::Vst3PathsChanged, this, [this] { rescan(); });

  if(qEnvironmentVariableIsEmpty("SCORE_DISABLE_AUDIOPLUGINS"))
  {
    rescan();
  }
}

void ApplicationPlugin::rescan()
{
  auto paths = context.settings<Media::Settings::Model>().getVst3Paths();

  // VST3_PATH
  if(QFileInfo vst_env_path{QString(qgetenv("VST3_PATH"))}; vst_env_path.isDir())
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
  rescan(paths);
}

static const QString& vst3PuppetPath()
{
  static const QString path = []() -> QString {
    auto app = QCoreApplication::instance()->applicationDirPath();
#if defined(__APPLE__)
    auto bundle_path
        = "/ossia-score-vst3puppet.app/Contents/MacOS/"
          "ossia-score-vst3puppet";
    QString bundle_vst3puppet = app + bundle_path;
    if(QFile::exists(bundle_vst3puppet))
      return bundle_vst3puppet;
    else if(QFile::exists(app + "/ossia-score-vst3puppet"))
      return QString(app + "/ossia-score-vst3puppet");
    else if(QFile::exists(app + "/../../ossia-score-vst3puppet"))
      return QString(app + "/../../ossia-score-vst3puppet");
    else if(QFile::exists(app + "/../../" + bundle_path))
      return QString(app + "/../../" + bundle_path);
    else
      return QStringLiteral("ossia-score-vst3puppet");
#else
    return app + "/ossia-score-vst3puppet";
#endif
  }();
  return path;
}

void ApplicationPlugin::rescan(const QStringList& paths)
{
#if QT_CONFIG(process)
  // 1. List all plug-ins in new paths
  QStringList exploredPaths;
  QSet<QString> newPlugins;
  for(const QString& dir : paths)
  {
    auto canonical_path = QDir{dir}.canonicalPath();
    if(exploredPaths.contains(canonical_path))
      continue;

    exploredPaths.push_back(canonical_path);

    QDirIterator it(
        dir, QStringList{default_filter}, default_format,
        QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while(it.hasNext())
    {
      QString plug = it.next();
      newPlugins.insert(plug);
    }
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
  QStringList toScan{newPlugins.begin(), newPlugins.end()};
  toScan.sort();

  m_scanner->setPuppet(vst3PuppetPath());
  m_scanner->scan(std::move(toScan));
#endif
}

void ApplicationPlugin::removeEntriesForPath(const QString& path)
{
  ossia::remove_erase_if(
      vst_infos, [&](const AvailablePlugin& i) { return i.path == path; });
}

void ApplicationPlugin::onScanned(const QString& path, const QJsonObject& obj)
{
  AvailablePlugin i = parseVst3Reply(path, obj);
  if(!i.isValid)
  {
    // No usable class in the reply (the puppet only emits audio-effect
    // classes, and classes with malformed UIDs are skipped): nothing we can
    // instantiate. Record it so the file is not rescanned on every startup.
    onScanFailed(path, QStringLiteral("no usable audio effect classes"));
    return;
  }

  removeEntriesForPath(path);
  vst_infos.push_back(std::move(i));
  schedulePersist();
}

void ApplicationPlugin::onScanFailed(const QString& path, const QString& reason)
{
  qDebug() << "VST3 scan failed for" << path << ":" << reason;

  AvailablePlugin i;
  i.path = path;
  i.name = "invalid";
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

VST3::Hosting::Module::Ptr ApplicationPlugin::getModule(const std::string& path)
{
  std::string err;
  auto it = modules.find(path);
  if(it != modules.end())
  {
    return it->second;
  }
  else
  {
    auto module = VST3::Hosting::Module::create(path, err);

    if(!module)
      throw std::runtime_error(fmt::format("Failed to load VST3 ({}) : {}", path, err));

    modules[path] = module;
    return module;
  }
}

std::pair<const AvailablePlugin*, const VST3::Hosting::ClassInfo*>
ApplicationPlugin::classInfo(const VST3::UID& uid) const noexcept
{
  // OPTIMIZEME with a small id -> {plugin, class} cache
  for(auto& plug : this->vst_infos)
  {
    for(auto& cls : plug.classInfo)
    {
      if(cls.ID() == uid)
        return {&plug, &cls};
    }
  }
  return {};
}

QString ApplicationPlugin::pathForClass(const VST3::UID& uid) const noexcept
{
  // OPTIMIZEME with the same cache than above
  for(auto& plug : this->vst_infos)
  {
    for(auto& cls : plug.classInfo)
    {
      if(cls.ID() == uid)
        return plug.path;
    }
  }
  return {};
}

std::optional<VST3::UID> ApplicationPlugin::uidForPathAndClassName(
    const QString& path, const QString& cls) const noexcept
{
  auto it
      = ossia::find_if(this->vst_infos, [&](auto& info) { return info.path == path; });
  if(it == this->vst_infos.end())
    return {};

  auto cls_it = ossia::find_if(it->classInfo, [&, n = cls.toStdString()](auto& info) {
    return info.name() == n;
  });
  if(cls_it == it->classInfo.end())
    return {};

  return cls_it->ID();
}
}
