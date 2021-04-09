#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <QDirIterator>

namespace Protocols
{

class LibraryDeviceEnumerator
    : public Device::DeviceEnumerator
{
public:
  std::string m_pattern;
  Device::ProtocolFactory::ConcreteKey m_key;
  std::function<QVariant(QByteArray)> m_createDeviceSettings;
  QDirIterator m_iterator;
  LibraryDeviceEnumerator(
      std::string pattern,
      QStringList extension,
      Device::ProtocolFactory::ConcreteKey k,
      std::function<QVariant(QByteArray)> createDev,
      const score::DocumentContext& ctx);

  void next();

  void enumerate(std::function<void(const Device::DeviceSettings&)> onDevice) const override;
};
}
