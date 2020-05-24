#include <Protocols/LibraryDeviceEnumerator.hpp>
#include <Library/LibrarySettings.hpp>

#include <score/document/DocumentContext.hpp>

#include <QFileInfo>
#include <QFile>
#include <QDirIterator>
#include <QTimer>

namespace Protocols
{
template<typename T>
static void findStringInFile(const QString& filepath, std::string_view req, T onSuccess)
{
  QFile f{filepath};
  if(f.open(QIODevice::ReadOnly)) {
    unsigned char* data = f.map(0, f.size());

    const char* cbegin = reinterpret_cast<char*>(data);
    const char* cend = cbegin + f.size();

    auto it = std::search(
          cbegin, cend,
          std::boyer_moore_searcher(req.begin(), req.end()));
    if(it != cend)
    {
      onSuccess(f);
    }

    f.unmap(data);
  }
}

LibraryDeviceEnumerator::LibraryDeviceEnumerator(
    std::string pattern,
    QString ext,
    Device::ProtocolFactory::ConcreteKey k,
    std::function<QVariant (QByteArray)> createDev,
    const score::DocumentContext& ctx)
  : m_pattern{std::move(pattern)}
  , m_extension{ext}
  , m_key{k}
  , m_createDeviceSettings{createDev}
  , m_iterator{ctx.app.settings<Library::Settings::Model>().getPath(), QDirIterator::Subdirectories | QDirIterator::FollowSymlinks}
{
  next();
}

void LibraryDeviceEnumerator::next()
{
  if(m_iterator.hasNext()) {
    auto filepath = m_iterator.next();
    if(QFileInfo fi{filepath}; fi.suffix() == m_extension)
    {
      findStringInFile(filepath, m_pattern.c_str(), [&] (QFile& f) {
        Device::DeviceSettings s;
        s.name = fi.baseName();
        s.protocol = m_key;
        s.deviceSpecificSettings = m_createDeviceSettings(f.readAll());
        deviceAdded(s);
      });
    }

    QTimer::singleShot(1, this, &LibraryDeviceEnumerator::next);
  }
}

void LibraryDeviceEnumerator::enumerate(std::function<void (const Device::DeviceSettings&)> onDevice) const
{
}

}
