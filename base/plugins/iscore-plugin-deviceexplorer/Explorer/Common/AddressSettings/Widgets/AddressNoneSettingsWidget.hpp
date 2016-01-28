#pragma once
#include "AddressSettingsWidget.hpp"
#include <Device/Address/AddressSettings.hpp>

class QWidget;

namespace DeviceExplorer
{
class AddressNoneSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressNoneSettingsWidget(QWidget* parent = nullptr);

        Device::AddressSettings getSettings() const override;

        void setSettings(const Device::AddressSettings& settings) override;
};

// MOVEME
class AddressImpulseSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressImpulseSettingsWidget(QWidget* parent = nullptr);

        Device::AddressSettings getSettings() const override;

        void setSettings(const Device::AddressSettings& settings) override;
};
}
