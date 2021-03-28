#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <Protocols/Artnet/ArtnetSpecificSettings.hpp>

#include <variant>
#include <verdigris>

class QLineEdit;
class QSpinBox;
class QTableWidget;
class QPushButton;

namespace Protocols
{

class ArtnetProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(ArtnetProtocolSettingsWidget)

public:
  ArtnetProtocolSettingsWidget(QWidget* parent = nullptr);
  virtual ~ArtnetProtocolSettingsWidget();
  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void updateTable();
  QLineEdit* m_deviceNameEdit{};
  QSpinBox* m_rate{};
  QTableWidget* m_fixturesWidget{};
  QPushButton* m_addFixture{};
  QPushButton* m_rmFixture{};
  std::vector<Artnet::Fixture> m_fixtures;
};
}
#endif
