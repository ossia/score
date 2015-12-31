#pragma once

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Device/Protocol/DeviceSettings.hpp>

class QLineEdit;
class QSpinBox;
class QWidget;

class LocalProtocolSettingsWidget final : public ProtocolSettingsWidget
{
    public:
        LocalProtocolSettingsWidget(QWidget* parent = nullptr);

    private:
        Device::DeviceSettings getSettings() const override;
        void setSettings(const Device::DeviceSettings& settings) override;
};

