#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/DefaultProtocolFactory.hpp>

namespace Protocols
{

struct LibmapperClientSpecificSettings
{
  QString id;
};
}

Q_DECLARE_METATYPE(Protocols::LibmapperClientSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::LibmapperClientSpecificSettings)

namespace Protocols
{

class LibmapperClientDevice final : public Device::OwningDeviceInterface
{
  W_OBJECT(LibmapperClientDevice)
public:
  LibmapperClientDevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);
  ~LibmapperClientDevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const ossia::net::network_context_ptr& m_ctx;
};

class LibmapperClientProtocolFactory final : public DefaultProtocolFactory
{
  SCORE_CONCRETE("708191fc-a901-414f-90c6-fef4a284330d")

  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  Device::DeviceEnumerators getEnumerators(const score::DocumentContext& ctx) const override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override;

  const Device::DeviceSettings& defaultSettings() const noexcept override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(
      const QVariant& data, const VisitorVariant& visitor) const override;

  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const noexcept override;
};

class LibmapperClientProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(LibmapperClientProtocolSettingsWidget)

public:
  LibmapperClientProtocolSettingsWidget(QWidget* parent = nullptr);
  virtual ~LibmapperClientProtocolSettingsWidget();

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  QLineEdit* m_deviceNameEdit{};
  Device::DeviceSettings m_settings;
};

}
