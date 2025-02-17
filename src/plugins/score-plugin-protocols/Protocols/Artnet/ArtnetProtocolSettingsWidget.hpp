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

  std::pair<int, int> universeRange() const noexcept;

private:
  void addLEDs(int mode);
  void addFixture(QString manufacturer, QString name);
  void addDimmer();
  void updateHosts(int protocolindex);
  void updateTable();

  void addressChanged(int row);
  void universeChanged(int row);

  QString newFixtureName(QString name);
  QLineEdit* m_deviceNameEdit{};
  QComboBox* m_host{};
  QSpinBox* m_rate{};
  QSpinBox* m_universe{};
  QSpinBox* m_universe_count{};
  QSpinBox* m_channels_per_universe{};
  QComboBox* m_transport{};
  QCheckBox* m_multicast{};
  QRadioButton* m_source{};
  QRadioButton* m_sink{};
  QTableWidget* m_fixturesWidget{};
  QPushButton* m_addFixture{};
  QPushButton* m_addGenericDimmer{};
  QPushButton* m_addGenericRGB{};
  QPushButton* m_addLEDStrip{};
  QPushButton* m_addLEDPane{};
  QPushButton* m_addLEDBox{};
  QPushButton* m_rmFixture{};
  std::vector<Artnet::Fixture> m_fixtures;
};
}
#endif
