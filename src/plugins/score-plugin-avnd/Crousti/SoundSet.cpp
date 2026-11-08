#include "SoundSet.hpp"

#include <Media/AudioDecoder.hpp>
#include <Media/Sound/Drop/SoundDrop.hpp>

#include <score/tools/NaturalSort.hpp>
#include <score/tools/ThreadPool.hpp>

#include <ossia-qt/invoke.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QPointer>
#include <QRegularExpression>

#include <algorithm>
#include <unordered_map>

namespace oscr
{
namespace
{
// Watching each file catches in-place rewrites, which do not touch the folder;
// past this many, only the folders are watched.
constexpr int max_watched_files = 512;

QStringList nameFilters(const SoundSetOptions& opts)
{
  QStringList exts = opts.extensions;
  if(exts.empty())
    for(const auto& e : Media::Sound::DropHandler{}.fileExtensions())
      exts.push_back(e);

  QStringList filters;
  for(const auto& e : exts)
    filters.push_back(QStringLiteral("*.") + e);
  return filters;
}

//! Relative paths of the folder's files, in natural order
QStringList listFiles(const QString& folder, const SoundSetOptions& opts)
{
  const QDir root{folder};
  QStringList files;
  QDirIterator it{
      folder, nameFilters(opts), QDir::Files | QDir::Readable,
      opts.recursive ? QDirIterator::Subdirectories | QDirIterator::FollowSymlinks
                     : QDirIterator::NoIteratorFlags};
  while(it.hasNext())
    files.push_back(root.relativeFilePath(it.next()));

  score::sortNaturally(files);
  return files;
}

//! A decoded copy of `e`: published entries are never modified
std::shared_ptr<SoundSetEntry> decodeEntry(const SoundSetEntry& e, int rate)
{
  auto res = std::make_shared<SoundSetEntry>();
  res->path = e.path;
  res->name = e.name;
  res->modified = e.modified;
  res->size = e.size;
  res->status = halp::sound_status::failed;

  if(auto dec = Media::AudioDecoder::decode_synchronous(e.path, rate))
  {
    auto& data = dec->second;
    if(!data.empty() && !data[0].empty())
    {
      res->data = std::move(data);
      for(auto& chan : res->data)
        res->channels.push_back(chan.data());
      res->rate = rate;
      res->status = halp::sound_status::ready;
    }
  }
  return res;
}

//! The folder's files; those unchanged since `previous` are kept as they are
std::shared_ptr<SoundSetSnapshot> listSnapshot(
    const QString& folder, const SoundSetOptions& opts,
    const SoundSetSnapshot* previous)
{
  std::unordered_map<std::string, std::shared_ptr<SoundSetEntry>> known;
  if(previous)
    for(auto& e : previous->entries)
      if(e->status == halp::sound_status::ready
         || e->status == halp::sound_status::failed)
        known.emplace(e->name, e);

  auto snap = std::make_shared<SoundSetSnapshot>();
  snap->folder = folder.toStdString();

  const QDir root{folder};
  for(const auto& rel : listFiles(folder, opts))
  {
    const QString path = root.absoluteFilePath(rel);
    const QFileInfo fi{path};
    const std::string name = rel.toStdString();

    if(auto it = known.find(name); it != known.end()
                                   && it->second->modified == fi.lastModified()
                                   && it->second->size == fi.size())
    {
      snap->entries.push_back(it->second);
      continue;
    }

    auto e = std::make_shared<SoundSetEntry>();
    e->path = path;
    e->name = name;
    e->modified = fi.lastModified();
    e->size = fi.size();
    e->status = opts.decoding == halp::sound_decoding::all
                    ? halp::sound_status::loading
                    : halp::sound_status::unloaded;
    snap->entries.push_back(std::move(e));
  }
  return snap;
}
}

void SoundSetSnapshot::make_views()
{
  views.clear();
  views.reserve(entries.size());
  for(auto& e : entries)
  {
    halp::sound_view v;
    v.name = e->name;
    v.status = e->status;
    if(e->status == halp::sound_status::ready)
    {
      v.data = e->channels.data();
      v.frames = int64_t(e->data[0].size());
      v.channels = int32_t(e->channels.size());
      v.rate = e->rate;
    }
    v.pins = &e->pins;
    v.requested = &e->requested;
    views.push_back(v);
  }
}

QStringList soundSetExtensions(std::string_view exts)
{
  QStringList res;
  for(auto& part : QString::fromUtf8(exts.data(), qsizetype(exts.size()))
                       .split(QRegularExpression{"[\\s,;]+"}, Qt::SkipEmptyParts))
  {
    while(part.startsWith('*') || part.startsWith('.'))
      part.remove(0, 1);
    if(!part.isEmpty())
      res.push_back(part.toLower());
  }
  return res;
}

SoundSetLoader::SoundSetLoader(
    SoundSetOptions opts, publish_function publish, dispose_function dispose,
    QObject* parent)
    : QObject{parent}
    , m_opts{std::move(opts)}
    , m_publish{std::move(publish)}
    , m_dispose{std::move(dispose)}
{
  // Changes on disk come in bursts: rescan once they settle
  m_rescan.setSingleShot(true);
  m_rescan.setInterval(250);
  connect(&m_rescan, &QTimer::timeout, this, &SoundSetLoader::scan);
  connect(
      &m_watcher, &QFileSystemWatcher::directoryChanged, this,
      &SoundSetLoader::scheduleScan);
  connect(
      &m_watcher, &QFileSystemWatcher::fileChanged, this,
      &SoundSetLoader::scheduleScan);

  m_reaper.setInterval(100);
  connect(&m_reaper, &QTimer::timeout, this, &SoundSetLoader::reap);
  m_reaper.start();

  if(m_opts.decoding == halp::sound_decoding::on_demand)
  {
    m_requests.setInterval(20);
    connect(&m_requests, &QTimer::timeout, this, &SoundSetLoader::decodeRequested);
    m_requests.start();
  }
}

SoundSetLoader::~SoundSetLoader()
{
  // The processor may still be reading the current snapshot, or pinned sounds
  // of older ones: they go to where they are freed once it stopped running.
  std::vector<std::shared_ptr<SoundSetSnapshot>> remaining = std::move(m_retired);
  if(m_current)
    remaining.push_back(std::move(m_current));
  if(!remaining.empty() && m_dispose)
    m_dispose(std::move(remaining));
}

void SoundSetLoader::setPath(const QString& path)
{
  QString folder = path;
  if(const QFileInfo fi{path}; fi.isFile())
    folder = fi.absolutePath();
  if(folder == m_folder && m_current)
    return;

  m_folder = folder;
  rewatch();
  scan();
}

void SoundSetLoader::scheduleScan()
{
  m_rescan.start();
}

void SoundSetLoader::scan()
{
  const uint64_t token = ++m_token;
  m_decoding.clear();

  if(m_folder.isEmpty() || !QFileInfo{m_folder}.isDir())
  {
    auto snap = std::make_shared<SoundSetSnapshot>();
    snap->folder = m_folder.toStdString();
    publishSnapshot(std::move(snap));
    return;
  }

  auto& pool = score::TaskPool::instance();
  pool.post([self = QPointer{this}, token, folder = m_folder, opts = m_opts,
             previous = m_current]() mutable {
    auto listed = listSnapshot(folder, opts, previous.get());
    previous.reset();

    const bool decode_all = opts.decoding == halp::sound_decoding::all;
    const bool to_decode = decode_all
                           && std::any_of(
                               listed->entries.begin(), listed->entries.end(),
                               [](auto& e) {
      return e->status == halp::sound_status::loading;
    });

    // The listing first: the files are there, the processor can tell how many
    if(!to_decode)
    {
      ossia::qt::run_async(qApp, [self, token, listed = std::move(listed)]() mutable {
        if(self)
          self->scanned(token, std::move(listed));
      });
      return;
    }

    auto decoded = std::make_shared<SoundSetSnapshot>();
    decoded->folder = listed->folder;
    decoded->entries = listed->entries;
    ossia::qt::run_async(qApp, [self, token, listed = std::move(listed)]() mutable {
      if(self)
        self->scanned(token, std::move(listed));
    });

    for(auto& e : decoded->entries)
      if(e->status == halp::sound_status::loading)
        e = decodeEntry(*e, opts.rate);

    ossia::qt::run_async(qApp, [self, token, decoded = std::move(decoded)]() mutable {
      if(self)
        self->scanned(token, std::move(decoded));
    });
  });
}

void SoundSetLoader::scanned(uint64_t token, std::shared_ptr<SoundSetSnapshot> snapshot)
{
  // A newer scan was started meanwhile
  if(token != m_token)
    return;
  publishSnapshot(std::move(snapshot));
  rewatch();
}

void SoundSetLoader::decodeRequested()
{
  if(!m_current)
    return;

  for(auto& e : m_current->entries)
  {
    if(e->status != halp::sound_status::unloaded
       || !e->requested.load(std::memory_order_relaxed))
      continue;
    if(std::find(m_decoding.begin(), m_decoding.end(), e) != m_decoding.end())
      continue;

    m_decoding.push_back(e);
    score::TaskPool::instance().post(
        [self = QPointer{this}, token = m_token, e, rate = m_opts.rate]() mutable {
      auto res = decodeEntry(*e, rate);
      e.reset();
      ossia::qt::run_async(qApp, [self, token, res = std::move(res)]() mutable {
        if(self)
          self->decoded(token, std::move(res));
      });
    });
  }
}

void SoundSetLoader::decoded(uint64_t token, std::shared_ptr<SoundSetEntry> entry)
{
  if(token != m_token || !m_current)
    return;

  std::erase_if(m_decoding, [&](auto& e) { return e->name == entry->name; });

  auto next = std::make_shared<SoundSetSnapshot>();
  next->folder = m_current->folder;
  next->entries = m_current->entries;
  for(auto& e : next->entries)
  {
    if(e->name == entry->name)
    {
      e = std::move(entry);
      publishSnapshot(std::move(next));
      return;
    }
  }
}

void SoundSetLoader::publishSnapshot(std::shared_ptr<SoundSetSnapshot> snapshot)
{
  snapshot->generation = ++m_generation;
  snapshot->make_views();

  auto previous = std::move(m_current);
  m_current = snapshot;
  if(m_publish)
    m_publish(std::move(snapshot), previous.get());
  if(previous)
    m_retired.push_back(std::move(previous));
}

void SoundSetLoader::reap()
{
  std::erase_if(m_retired, [](const std::shared_ptr<SoundSetSnapshot>& snap) {
    if(!snap->superseded.load(std::memory_order_acquire))
      return false;
    // The sounds only this snapshot holds must not be pinned anymore; the
    // others live on in newer snapshots.
    for(auto& e : snap->entries)
      if(e.use_count() == 1 && e->pins.load(std::memory_order_acquire) > 0)
        return false;
    return true;
  });
}

void SoundSetLoader::rewatch()
{
  if(auto paths = m_watcher.directories() + m_watcher.files(); !paths.empty())
    m_watcher.removePaths(paths);
  if(m_folder.isEmpty() || !QFileInfo{m_folder}.isDir())
    return;

  QStringList paths{m_folder};
  if(m_opts.recursive)
  {
    QDirIterator it{
        m_folder, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories};
    while(it.hasNext())
      paths.push_back(it.next());
  }
  if(m_current && std::ssize(m_current->entries) <= max_watched_files)
    for(auto& e : m_current->entries)
      paths.push_back(e->path);
  m_watcher.addPaths(paths);
}
}
