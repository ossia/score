#include <Library/LibrarySettings.hpp>
#include <Protocols/LibraryDeviceEnumerator.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/File.hpp>
#include <score/tools/FindStringInFile.hpp>

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include <memory>
#include <QTimer>

namespace Protocols
{
LibraryDeviceEnumerator::LibraryDeviceEnumerator(
    std::string pattern, QStringList ext, Device::ProtocolFactory::ConcreteKey k,
    std::function<QVariant(QByteArray)> createDev, const score::DocumentContext& ctx)
    : LibraryDeviceEnumerator{
          std::move(pattern), std::move(ext), k,
          [createDev = std::move(createDev)](QByteArray arr, const QString&) {
            return createDev(std::move(arr));
          }, ctx}
{
}

LibraryDeviceEnumerator::LibraryDeviceEnumerator(
    std::string pattern, QStringList ext, Device::ProtocolFactory::ConcreteKey k,
    std::function<QVariant(QByteArray, const QString&)> createDev,
    const score::DocumentContext& ctx)
    : m_pattern{std::move(pattern)}
    , m_key{k}
    , m_createDeviceSettings{std::move(createDev)}
{
  m_watch.setWatchedFolder(
      ctx.app.settings<Library::Settings::Model>().getPackagesPath().toStdString());

  // Matching happens on a scanning thread, so it works from its own copy of
  // what it needs rather than reaching into this object, which may be gone by
  // then. What it hands back runs on the gui thread, where the scan's guard on
  // this object already decides whether it runs at all.
  struct matcher_data
  {
    std::string pattern;
    Device::ProtocolFactory::ConcreteKey key;
    std::function<QVariant(QByteArray, const QString&)> make;
  };
  auto matcher
      = std::make_shared<matcher_data>(m_pattern, m_key, m_createDeviceSettings);

  for(auto& e : ext)
  {
    score::RecursiveWatch::AsyncCallbacks cb;
    cb.filter = [this, m = matcher](std::string_view path) -> std::function<void()> {
      const QString filepath = QString::fromUtf8(path.data(), path.length());

      std::function<void()> result;
      score::findStringInFile(filepath, m->pattern.c_str(), [&](QFile& f) {
        Device::DeviceSettings s;
        s.name = QFileInfo{filepath}.baseName();
        s.protocol = m->key;
        s.deviceSpecificSettings = m->make(score::mapAsByteArray(f), filepath);
        result = [this, s = std::move(s)]() mutable { deviceAdded(s.name, s); };
      });
      return result;
    };
    m_watch.registerWatch(e.toStdString(), std::move(cb));
  }

  // Done delayed to leave the time to calling code to connect to deviceAdded, etc.
  QTimer::singleShot(1, this, [this] { m_watch.scanAsync(this); });
}

void LibraryDeviceEnumerator::next(std::string_view path)
{
  QString filepath = QString::fromUtf8(path.data(), path.length());

  score::findStringInFile(filepath, m_pattern.c_str(), [&](QFile& f) {
    Device::DeviceSettings s;
    s.name = QFileInfo{filepath}.baseName();
    s.protocol = m_key;
    s.deviceSpecificSettings
        = m_createDeviceSettings(score::mapAsByteArray(f), filepath);
    deviceAdded(s.name, s);
  });
}

void LibraryDeviceEnumerator::enumerate(
    std::function<void(const QString&, const Device::DeviceSettings&)> onDevice) const
{
}

SubfolderDeviceEnumerator::SubfolderDeviceEnumerator(
    QStringList roots, Device::ProtocolFactory::ConcreteKey k, func_type createDev,
    const score::DocumentContext& ctx)
    : m_key{k}
    , m_createDeviceSettings{createDev}
{
  for(const auto& root : roots)
  {
    QTimer::singleShot(10, this, [this, root, n = roots.size()] {
      QDirIterator it{
          root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags};
      while(it.hasNext())
      {
        for(auto spec : m_createDeviceSettings(it.next()))
        {
          Device::DeviceSettings s;
          s.name = spec.first;
          s.protocol = m_key;
          s.deviceSpecificSettings = std::move(spec.second);
          deviceAdded(s.name, s);
        }
      }

      m_finished++;
      if(m_finished == n)
        this->sort();
    });
  }
}

void SubfolderDeviceEnumerator::next(std::string_view path) { }

void SubfolderDeviceEnumerator::enumerate(
    std::function<void(const QString&, const Device::DeviceSettings&)> onDevice) const
{
}

}
