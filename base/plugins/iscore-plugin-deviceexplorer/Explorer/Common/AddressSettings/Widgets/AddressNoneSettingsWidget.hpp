#pragma once
#include "AddressSettingsWidget.hpp"
#include <Device/Address/AddressSettings.hpp>

class QWidget;

class AddressImpulseSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressImpulseSettingsWidget(QWidget* parent = nullptr);

        iscore::AddressSettings getSettings() const override;

        void setSettings(const iscore::AddressSettings& settings) override;
};
