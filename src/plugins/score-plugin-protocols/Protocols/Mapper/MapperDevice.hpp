#pragma once

#if __has_include(<QQmlEngine>)
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <Explorer/DefaultProtocolFactory.hpp>

#include <verdigris>

Q_DECLARE_METATYPE(std::vector<ossia::net::node_base*>)
W_REGISTER_ARGTYPE(std::vector<ossia::net::node_base*>)
class JSEdit;
namespace Protocols
{
class Mapper : public QObject
{
};

struct MapperSpecificSettings
{
  QString text;
};

class MapperProtocolFactory final : public Protocols::DefaultProtocolFactory
{
  SCORE_CONCRETE("910e2d87-a087-430d-b725-c988fe2bea01")

public:
  ~MapperProtocolFactory();

private:
  QString prettyName() const override;

  Device::DeviceInterface*
  makeDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx) override;

  const Device::DeviceSettings& defaultSettings() const override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor)
      const override;

  bool checkCompatibility(const Device::DeviceSettings& a, const Device::DeviceSettings& b)
      const override;
};

class MapperProtocolSettingsWidget : public Device::ProtocolSettingsWidget
{
public:
  MapperProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  void setDefaults();

protected:
  QLineEdit* m_name{};
  JSEdit* m_codeEdit{};
};
}

Q_DECLARE_METATYPE(Protocols::MapperSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::MapperSpecificSettings)
#endif
