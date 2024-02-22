#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/Artnet/ArtnetSpecificSettings.hpp>

#include <verdigris>

class QLineEdit;
class QSpinBox;
class QTableWidget;
class QPushButton;
class QRadioButton;

namespace Protocols
{

class ArtnetProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(ArtnetProtocolSettingsWidget)

public:
  explicit ArtnetProtocolSettingsWidget(QWidget* parent = nullptr);
  virtual ~ArtnetProtocolSettingsWidget();
  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void updateHosts(int protocolindex);
  void updateTable();
  QLineEdit* m_deviceNameEdit{};
  QComboBox* m_host{};
  QSpinBox* m_rate{};
  QSpinBox* m_universe{};
  QComboBox* m_transport{};
  QRadioButton* m_source{};
  QRadioButton* m_sink{};
  QTableWidget* m_fixturesWidget{};
  QPushButton* m_addFixture{};
  QPushButton* m_rmFixture{};
  std::vector<Artnet::Fixture> m_fixtures;
};
}
#endif
