#pragma once

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Device/Protocol/DeviceSettings.hpp>

class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QWidget;

namespace Engine
{
namespace Network
{
class HTTPProtocolSettingsWidget :
        public Device::ProtocolSettingsWidget
{
    public:
        HTTPProtocolSettingsWidget(QWidget* parent = nullptr);

        Device::DeviceSettings getSettings() const override;

        void setSettings(const Device::DeviceSettings& settings) override;

    protected:
        void buildGUI();

        void setDefaults();

    protected:
        QLineEdit* m_deviceNameEdit;
        QPlainTextEdit* m_codeEdit;

};
}
}
