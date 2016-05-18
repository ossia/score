#pragma once
#include "AddressSettingsWidget.hpp"
#include <Device/Address/AddressSettings.hpp>

namespace Explorer
{
class AddressImpulseSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressImpulseSettingsWidget(QWidget* parent = nullptr);

        Device::AddressSettings getSettings() const override;

        void setSettings(const Device::AddressSettings& settings) override;
};
}
