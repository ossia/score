#include <Library/LibrarySettings.hpp>
#include <Protocols/LibraryDeviceEnumerator.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/File.hpp>
#include <score/tools/FindStringInFile.hpp>

#include <QFile>
#include <QFileInfo>
#include <QTimer>

namespace Protocols
{
LibraryDeviceEnumerator::LibraryDeviceEnumerator(
    std::string pattern,
    QStringList ext,
    Device::ProtocolFactory::ConcreteKey k,
    std::function<QVariant(QByteArray)> createDev,
    const score::DocumentContext& ctx)
    : m_pattern{std::move(pattern)}
    , m_key{k}
    , m_createDeviceSettings{createDev}
{
  m_watch.setWatchedFolder(ctx.app.settings<Library::Settings::Model>().getPackagesPath().toStdString());

  score::RecursiveWatch::Callbacks cb;
  cb.added = [this] (std::string_view path) {
    next(path);
  };

  for(auto& e : ext)
    m_watch.registerWatch(e.toStdString(), cb);

  // Done delayed to leave the time to calling code to connect to deviceAdded, etc.
  QTimer::singleShot(1, this, [this] { m_watch.scan(); });
}

void LibraryDeviceEnumerator::next(std::string_view path)
{
  QString filepath = QString::fromUtf8(path.data(), path.length());

  score::findStringInFile(filepath, m_pattern.c_str(), [&](QFile& f) {
    Device::DeviceSettings s;
    s.name = QFileInfo{filepath}.baseName();
    s.protocol = m_key;
    s.deviceSpecificSettings
        = m_createDeviceSettings(score::mapAsByteArray(f));
    deviceAdded(s);
  });

}

void LibraryDeviceEnumerator::enumerate(
    std::function<void(const Device::DeviceSettings&)> onDevice) const
{
}

}
