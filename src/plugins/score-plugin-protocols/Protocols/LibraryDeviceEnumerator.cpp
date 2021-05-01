#include <Library/LibrarySettings.hpp>
#include <Protocols/LibraryDeviceEnumerator.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/tools/File.hpp>
#include <score/tools/FindStringInFile.hpp>

#include <QDirIterator>
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
    , m_iterator{
          ctx.app.settings<Library::Settings::Model>().getPath(),
          ext,
          QDir::Files,
          QDirIterator::Subdirectories | QDirIterator::FollowSymlinks}
{
  QTimer::singleShot(1, this, &LibraryDeviceEnumerator::next);
}

void LibraryDeviceEnumerator::next()
{
  if (m_iterator.hasNext())
  {
    const auto& filepath = m_iterator.next();

    {
      score::findStringInFile(filepath, m_pattern.c_str(), [&](QFile& f) {
        Device::DeviceSettings s;
        s.name = QFileInfo{filepath}.baseName();
        s.protocol = m_key;
        s.deviceSpecificSettings
            = m_createDeviceSettings(score::mapAsByteArray(f));
        deviceAdded(s);
      });
    }

    QTimer::singleShot(1, this, &LibraryDeviceEnumerator::next);
  }
}

void LibraryDeviceEnumerator::enumerate(
    std::function<void(const Device::DeviceSettings&)> onDevice) const
{
}

}
