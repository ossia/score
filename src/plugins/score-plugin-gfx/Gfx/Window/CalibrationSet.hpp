#pragma once
#include <Gfx/Graph/WarpImport.hpp>
#include <Gfx/Window/WindowSettings.hpp>

#include <score/tools/ProjectFiles.hpp>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <vector>

namespace Gfx
{
//! An output-stage file found next to a calibration `name` (`name` + one of the
//! suffixes): the first suffix that exists wins.
struct CalibrationFileSlot
{
  QString OutputMaps::* member;
  std::vector<QString> suffixes;
  //! Written by a calibration process, which then lists it (calibrationListing).
  bool listed{true};
};

inline const std::vector<CalibrationFileSlot>& calibrationFileSlots()
{
  static const std::vector<CalibrationFileSlot> slots{
      {&OutputMaps::alpha, {QStringLiteral(".alpha.pfm")}},
      {&OutputMaps::beta, {QStringLiteral(".beta.pfm")}},
      {&OutputMaps::lut, {QStringLiteral(".cube"), QStringLiteral(".lut.cube")}, false},
      {&OutputMaps::response, {QStringLiteral(".response.json")}},
      {&OutputMaps::compensation, {QStringLiteral(".comp.json")}},
      {&OutputMaps::warp, {QStringLiteral(".warp.pfm")}}};
  return slots;
}

//! `dir/name` of any file of a calibration: name.json, name.xyz.pfm,
//! name.alpha.pfm, ... or the bare name.
inline QString calibrationBase(const QString& path)
{
  if(path.isEmpty())
    return {};
  static const QString suffixes[]{
      QStringLiteral(".warp.valid.pfm"), QStringLiteral(".warp.pfm"),
      QStringLiteral(".blend.json"), QStringLiteral(".response.json"),
      QStringLiteral(".comp.json"),  QStringLiteral(".json"),
      QStringLiteral(".xyz.pfm"),    QStringLiteral(".valid.pfm"),
      QStringLiteral(".alpha.pfm"),  QStringLiteral(".beta.pfm"),
      QStringLiteral(".lum.pfm"),    QStringLiteral(".black.pfm"),
      QStringLiteral(".lut.cube"),   QStringLiteral(".cube"),
      QStringLiteral(".pfm")};
  QString p = QDir::fromNativeSeparators(path);
  for(const auto& s : suffixes)
    if(p.endsWith(s, Qt::CaseInsensitive))
      return p.left(p.size() - s.size());
  return p;
}

//! Projector k (0 based) of a calibration set saved under `base`:
//! `base`, `base_p1`, `base_p2`, ... as Structured Light Calibration names them.
inline QString calibrationSetName(const QString& base, int k)
{
  return k <= 0 ? base : base + QStringLiteral("_p") + QString::number(k);
}

//! The first projector's name of the set any member `path` belongs to.
inline QString calibrationSetBase(const QString& path)
{
  const QString base = calibrationBase(path);
  static const QRegularExpression suffix{QStringLiteral("_p[0-9]+$")};
  if(const auto m = suffix.match(base); m.hasMatch())
    return base.left(m.capturedStart());
  return base;
}

//! The file names the last run of the calibration `base` wrote: the "files" of
//! name.json, and the alphaMap / betaMap of a name.blend.json at least as
//! recent. Empty when name.json lists nothing: then any file on disk counts.
inline std::optional<QStringList> calibrationListing(const QString& base)
{
  if(base.isEmpty())
    return {};
  QFile f{base + QStringLiteral(".json")};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  const auto o = QJsonDocument::fromJson(f.readAll()).object();
  const auto files = o.value(QStringLiteral("files"));
  if(!files.isArray())
    return {};
  QStringList out;
  for(const auto& v : files.toArray())
    out.push_back(v.toString());
  const QFileInfo json{base + QStringLiteral(".json")};
  const QFileInfo blend{base + QStringLiteral(".blend.json")};
  if(QFile b{blend.filePath()};
     blend.lastModified() >= json.lastModified() && b.open(QIODevice::ReadOnly))
  {
    const auto bo = QJsonDocument::fromJson(b.readAll()).object();
    for(const char* k : {"alphaMap", "betaMap"})
      if(const auto v = bo.value(QLatin1String(k)).toString(); !v.isEmpty())
        out.push_back(v);
  }
  return out;
}

inline QString findCalibrationFile(
    const QString& base, const CalibrationFileSlot& slot,
    const std::optional<QStringList>& listing = {})
{
  if(base.isEmpty())
    return {};
  for(const auto& s : slot.suffixes)
    if(QFileInfo f{base + s}; f.isFile())
    {
      if(slot.listed && listing && !listing->contains(f.fileName(), Qt::CaseInsensitive))
        continue;
      return f.filePath();
    }
  return {};
}

inline bool isCalibrationFileOf(
    const QString& file, const QString& base, const CalibrationFileSlot& slot)
{
  if(file.isEmpty() || base.isEmpty())
    return false;
  const QString f = QDir::fromNativeSeparators(file);
  if(!f.startsWith(base))
    return false;
  for(const auto& s : slot.suffixes)
    if(f.size() == base.size() + s.size() && f.endsWith(s, Qt::CaseInsensitive))
      return true;
  return false;
}

//! Whether anything of a calibration `base` exists on disk.
inline bool calibrationExists(const QString& base)
{
  if(base.isEmpty())
    return false;
  if(QFileInfo{base + ".json"}.isFile() || QFileInfo{base + ".xyz.pfm"}.isFile())
    return true;
  for(const auto& slot : calibrationFileSlots())
    if(!findCalibrationFile(base, slot).isEmpty())
      return true;
  return false;
}

//! Points `m` at the calibration of `path` and fills every slot that has a
//! file for it, among those its last run wrote (calibrationListing). A slot without a file is cleared when it held the previous
//! calibration's file, kept otherwise. An empty path detaches the calibration.
//! Returns the number of files assigned.
inline int applyCalibration(OutputMaps& m, const QString& path)
{
  const QString oldBase = calibrationBase(m.calibration);
  const QString base = calibrationBase(path);
  m.calibration = base.isEmpty() ? QString{} : base + QStringLiteral(".json");
  const auto listing = calibrationListing(base);
  int n = 0;
  for(const auto& slot : calibrationFileSlots())
  {
    QString& v = m.*slot.member;
    if(const QString f = findCalibrationFile(base, slot, listing); !f.isEmpty())
    {
      v = f;
      n++;
    }
    else if(isCalibrationFileOf(v, oldBase, slot))
      v.clear();
  }
  return n;
}

//! Re-resolves the slots that follow the calibration of `m` (empty, or
//! holding a file of that calibration): a file that appeared is filled in, one
//! that disappeared is cleared. Slots set to another file are left alone.
//! Returns whether `m` changed.
inline bool refreshCalibration(OutputMaps& m)
{
  const QString base = calibrationBase(m.calibration);
  if(base.isEmpty())
    return false;
  bool changed = false;
  const auto listing = calibrationListing(base);
  for(const auto& slot : calibrationFileSlots())
  {
    QString& v = m.*slot.member;
    if(!v.isEmpty() && !isCalibrationFileOf(v, base, slot))
      continue;
    const QString f = findCalibrationFile(base, slot, listing);
    if(f != v)
    {
      v = f;
      changed = true;
    }
  }
  return changed;
}

//! `m` with every file located against `roots`, as score::locateFilePath does.
inline OutputMaps locatedMaps(OutputMaps m, const score::PathRoots& roots)
{
  for(QString* f : {&m.warp, &m.alpha, &m.beta, &m.lut, &m.response, &m.compensation,
                    &m.calibration})
    if(!f->isEmpty())
      *f = score::locateFilePath(*f, roots);
  return m;
}

//! `m` with every file under a root as a <PROJECT>: / <LIBRARY>: path, as
//! score::relativizeFilePath does; an MPCDI region stays attached to its archive.
inline OutputMaps relativizedMaps(OutputMaps m, const score::PathRoots& roots)
{
  for(QString* f : {&m.warp, &m.alpha, &m.beta, &m.lut, &m.response, &m.compensation,
                    &m.calibration})
  {
    if(f->isEmpty())
      continue;
    const QString archive = score::gfx::mapArchivePath(*f);
    *f = score::relativizeFilePath(archive, roots) + f->mid(archive.size());
  }
  return m;
}

//! The files of `m` that cannot be found, as "name: path" lines.
inline QStringList missingMapFiles(const OutputMaps& m, const score::PathRoots& roots)
{
  QStringList out;
  const std::pair<const char*, const QString*> fields[]{
      {"Calibration", &m.calibration}, {"UV warp", &m.warp},
      {"Alpha", &m.alpha},             {"Beta", &m.beta},
      {"Colour LUT", &m.lut},          {"Response", &m.response},
      {"Compensation", &m.compensation}};
  for(const auto& [name, f] : fields)
  {
    if(f->isEmpty())
      continue;
    const QString path = score::gfx::mapArchivePath(score::locateFilePath(*f, roots));
    if(!QFileInfo{path}.isFile())
      out.push_back(QStringLiteral("%1: %2").arg(QLatin1String(name), path));
  }
  return out;
}

//! The files `m` reads, in a fixed order.
inline QStringList mapFiles(const OutputMaps& m)
{
  QStringList out;
  for(const QString* f : {&m.warp, &m.alpha, &m.beta, &m.lut, &m.response, &m.compensation})
    if(!f->isEmpty())
      out.push_back(*f);
  return out;
}

//! Changes when any file `m` reads is modified, re-created or removed.
inline QString mapsSignature(const OutputMaps& m)
{
  QString s;
  for(const auto& f : mapFiles(m))
    s += score::gfx::mapFileStamp(f) + QLatin1Char(';');
  return s;
}

//! Watches calibration files through their folders too, so that re-created
//! files are seen; calls back once events have stopped for `delayMs`.
class CalibrationWatcher
{
public:
  explicit CalibrationWatcher(std::function<void()> onChanged, int delayMs = 300)
      : m_onChanged{std::move(onChanged)}
  {
    m_timer.setSingleShot(true);
    m_timer.setInterval(delayMs);
    QObject::connect(&m_timer, &QTimer::timeout, &m_timer, [this] {
      if(m_onChanged)
        m_onChanged();
    });
    auto kick = [this](const QString&) { m_timer.start(); };
    QObject::connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_timer, kick);
    QObject::connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_timer, kick);
  }

  //! Watches exactly `files`, those that do not exist yet included, through
  //! their folders.
  void setFiles(const QStringList& files)
  {
    QStringList want, dirs;
    for(const auto& f : files)
    {
      if(f.isEmpty())
        continue;
      const QFileInfo fi{f};
      if(fi.isFile())
        want.push_back(fi.absoluteFilePath());
      const QString d = fi.absolutePath();
      if(QFileInfo{d}.isDir() && !dirs.contains(d))
        dirs.push_back(d);
    }
    want += dirs;
    const QStringList have = m_watcher.files() + m_watcher.directories();
    QStringList stale;
    for(const auto& h : have)
      if(!want.contains(h))
        stale.push_back(h);
    if(!stale.isEmpty())
      m_watcher.removePaths(stale);
    QStringList add;
    for(const auto& w : want)
      if(!have.contains(w))
        add.push_back(w);
    if(!add.isEmpty())
      m_watcher.addPaths(add);
  }

  QStringList watched() const { return m_watcher.files() + m_watcher.directories(); }

private:
  QFileSystemWatcher m_watcher;
  QTimer m_timer;
  std::function<void()> m_onChanged;
};

//! What a device watches for `maps`: the files they read and those their
//! calibrations may gain.
inline QStringList watchedCalibrationFiles(const std::vector<OutputMaps>& maps)
{
  QStringList out;
  for(const auto& m : maps)
  {
    for(const auto& f : mapFiles(m))
      out.push_back(score::gfx::mapArchivePath(f));
    if(const QString base = calibrationBase(m.calibration); !base.isEmpty())
    {
      out.push_back(base + QStringLiteral(".json"));
      out.push_back(base + QStringLiteral(".blend.json"));
      for(const auto& slot : calibrationFileSlots())
        for(const auto& s : slot.suffixes)
          out.push_back(base + s);
    }
  }
  return out;
}
}
