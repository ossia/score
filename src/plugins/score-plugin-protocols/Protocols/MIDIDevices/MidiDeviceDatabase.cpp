#include "MidiDeviceDatabase.hpp"

#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

namespace Protocols::MIDIDevices
{
namespace
{
constexpr auto mapSuffix = QLatin1String{".midimap.json"};

//! "Yamaha_PLG100_XG_Expansion.midimap.json" is "Yamaha PLG100 XG Expansion",
//! for a document that does not name itself.
QString nameFromFile(const QString& path)
{
  auto name = QFileInfo{path}.fileName();
  name.chop(mapSuffix.size());
  return name.replace('_', ' ').simplified();
}

//! "Korg M1" of the brand "Korg" is "M1"; "Korgasmatron" stays whole.
QString withoutBrand(const QString& model, const QString& brand)
{
  if(brand.isEmpty() || !model.startsWith(brand, Qt::CaseInsensitive))
    return model;

  const auto separator
      = [](QChar c) { return c.isSpace() || c == '-' || c == '_' || c == ':'; };

  auto rest = model.mid(brand.size());
  if(!rest.isEmpty() && !separator(rest.front()))
    return model;

  while(!rest.isEmpty() && separator(rest.front()))
    rest.remove(0, 1);

  return rest.isEmpty() ? model : rest;
}

/**
 * How much of a file the scan reads before giving up on finding its header.
 *
 * The header ends at the first control, so this only has to cover a document's
 * own fields; the whole corpus fits in a fraction of it, and a file whose header
 * is longer is read in full rather than skipped.
 */
constexpr qint64 headerPrefix = 32768;

bool lessThan(const DeviceEntry& a, const DeviceEntry& b) noexcept
{
  const auto cmp = [](const std::string& x, const std::string& y) {
    return QString::compare(
        QString::fromStdString(x), QString::fromStdString(y), Qt::CaseInsensitive);
  };

  if(const int m = cmp(a.header.manufacturer, b.header.manufacturer); m != 0)
    return m < 0;
  if(const int d = cmp(a.header.model, b.header.model); d != 0)
    return d < 0;
  return cmp(a.header.preset.name, b.header.preset.name) < 0;
}

std::optional<std::string> readFile(const QString& path, qint64 max)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return std::nullopt;

  const auto bytes = max > 0 ? f.read(max) : f.readAll();
  return std::string{bytes.constData(), std::size_t(bytes.size())};
}
}

QString DeviceEntry::name() const
{
  auto name = namesSomething(header.model) ? QString::fromStdString(header.model)
                                           : nameFromFile(file);
  name = withoutBrand(name, QString::fromStdString(header.manufacturer));

  if(!header.preset.name.empty())
    name += " (" + QString::fromStdString(header.preset.name) + ")";
  return name;
}

QString DeviceEntry::label() const
{
  const auto brand = QString::fromStdString(header.manufacturer);
  if(!namesSomething(header.manufacturer))
    return name();
  return brand + ": " + name();
}

std::vector<QString> libraryPaths()
{
  const auto packages
      = score::AppContext().settings<Library::Settings::Model>().getPackagesPath();

  std::vector<QString> roots;

  const auto consider = [&roots](const QString& dir) {
    if(QFileInfo::exists(dir))
      roots.push_back(QDir{dir}.absolutePath());
  };

  consider(packages + "/midi-device-maps/maps");

  // A package of the user's own: <packages>/user, <packages>/default, ...
  QDirIterator sub{
      packages, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags};
  while(sub.hasNext())
    consider(sub.next() + "/midi-device-maps/maps");

  // Stable order: a user root is meant to shadow a shipped one.
  std::sort(roots.begin(), roots.end());
  roots.erase(std::unique(roots.begin(), roots.end()), roots.end());
  return roots;
}

Database& Database::instance()
{
  static Database db;
  return db;
}

Database::Database()
{
  rescan();
}

void Database::rescan()
{
  m_devices.clear();

  const auto add = [this](const QString& path, const QString& source) {
    auto head = readFile(path, headerPrefix);
    if(!head)
      return;

    auto parsed = parseDeviceMapHeader(*head);

    // A header longer than the prefix is rare enough to be worth one more read
    // rather than a bigger one for every file.
    if(!parsed && head->size() == std::size_t(headerPrefix))
    {
      if(const auto whole = readFile(path, 0))
        parsed = parseDeviceMapHeader(*whole);
    }
    if(!parsed)
      return;

    m_devices.push_back(DeviceEntry{
        path, source, source + "/" + QFileInfo{path}.fileName(), std::move(*parsed)});
  };

  for(const auto& root : libraryPaths())
  {
    QDirIterator sources{
        root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::FollowSymlinks};
    while(sources.hasNext())
    {
      const QDir dir{sources.next()};
      for(const auto& file :
          dir.entryList({"*" + mapSuffix}, QDir::Files | QDir::Readable, QDir::Name))
        add(dir.absoluteFilePath(file), dir.dirName());
    }
  }

  std::sort(m_devices.begin(), m_devices.end(), lessThan);
}

const DeviceEntry* Database::find(const QString& identity) const noexcept
{
  const auto it = std::find_if(
      m_devices.begin(), m_devices.end(),
      [&](const DeviceEntry& e) { return e.identity == identity; });
  return it != m_devices.end() ? &*it : nullptr;
}

std::optional<DeviceMap> Database::load(const DeviceEntry& entry)
{
  const auto text = readFile(entry.file, 0);
  if(!text)
    return std::nullopt;
  return parseDeviceMap(*text);
}

}
