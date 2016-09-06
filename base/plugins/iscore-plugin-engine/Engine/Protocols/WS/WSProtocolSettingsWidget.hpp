#pragma once

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Device/Protocol/DeviceSettings.hpp>
class QLineEdit;
class JSEdit;
class QSpinBox;
class QWidget;

namespace Engine
{
namespace Network
{
class WSProtocolSettingsWidget :
        public Device::ProtocolSettingsWidget
{
    public:
        WSProtocolSettingsWidget(QWidget* parent = nullptr);

        Device::DeviceSettings getSettings() const override;

        void setSettings(const Device::DeviceSettings& settings) override;

    protected:
        void setDefaults();

    protected:
        QLineEdit* m_deviceNameEdit{};
        QLineEdit* m_addressNameEdit{};
        JSEdit* m_codeEdit{};

};
}
}
