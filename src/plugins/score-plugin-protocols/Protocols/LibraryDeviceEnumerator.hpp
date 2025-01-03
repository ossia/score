#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>

#include <score/tools/RecursiveWatch.hpp>

#include <score_plugin_protocols_export.h>

namespace Protocols
{

class SCORE_PLUGIN_PROTOCOLS_EXPORT LibraryDeviceEnumerator
    : public Device::DeviceEnumerator
{
public:
  std::string m_pattern;
  Device::ProtocolFactory::ConcreteKey m_key;
  std::function<QVariant(QByteArray)> m_createDeviceSettings;
  score::RecursiveWatch m_watch;

  LibraryDeviceEnumerator(
      std::string pattern, QStringList extension, Device::ProtocolFactory::ConcreteKey k,
      std::function<QVariant(QByteArray)> createDev, const score::DocumentContext& ctx);

  void next(std::string_view path);

  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)>
                     onDevice) const override;
};

class SCORE_PLUGIN_PROTOCOLS_EXPORT SubfolderDeviceEnumerator
    : public Device::DeviceEnumerator
{
public:
  using ret_type = std::vector<std::pair<QString, QVariant>>;
  using func_type = std::function<ret_type(QString)>;
  Device::ProtocolFactory::ConcreteKey m_key;
  func_type m_createDeviceSettings;

  SubfolderDeviceEnumerator(
      QStringList rootFolder, Device::ProtocolFactory::ConcreteKey k,
      func_type createDev, const score::DocumentContext& ctx);

  void next(std::string_view path);

  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)>
                     onDevice) const override;

  int m_finished = 0;
};
}
