#pragma once
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>
#include <Device/Address/AddressSettings.hpp>

namespace Explorer
{
template<typename T>
class AddressArraySettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressArraySettingsWidget(QWidget* parent = nullptr)
            : AddressSettingsWidget(parent)
        {
            // TODO for each value, display the corresopnding settings widget (with min/max/etc...).
            // In order to do this properly,
            // we have to separate the global and per-tuple settings widgets...
        }

        Device::AddressSettings getSettings() const override
        {
            auto settings = getCommonSettings();
            settings.value.val = T{};
            return settings;
        }

        void setSettings(const Device::AddressSettings& settings) override
        {
            setCommonSettings(settings);
        }
};
}
