#pragma once
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <State/Widgets/Values/VecWidgets.hpp>

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

        Device::AddressSettings getDefaultSettings() const override
        {
            return {};
        }
};
template<int N>
class AddressVecSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressVecSettingsWidget(QWidget* parent = nullptr)
            : AddressSettingsWidget(parent)
        {
        }

        Device::AddressSettings getSettings() const override
        {
            auto settings = getCommonSettings();
            settings.value.val = std::array<float, N>{};
            return settings;
        }

        void setSettings(const Device::AddressSettings& settings) override
        {
            setCommonSettings(settings);
        }

        Device::AddressSettings getDefaultSettings() const override
        {
            return {};
        }


        State::CharValueWidget* m_valueEdit{};
        State::CharDomainWidget* m_domainEdit{};
};
}
