#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <wobjectdefs.h>

class QLineEdit;
class QComboBox;

namespace Engine::Network {

class ArtnetProtocolSettingsWidget final
    : public Device::ProtocolSettingsWidget
{
  W_OBJECT(ArtnetProtocolSettingsWidget)

public:
  ArtnetProtocolSettingsWidget(QWidget* parent = nullptr);
  virtual ~ArtnetProtocolSettingsWidget();
  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

protected:
  QLineEdit* m_deviceNameEdit{};
};

}
