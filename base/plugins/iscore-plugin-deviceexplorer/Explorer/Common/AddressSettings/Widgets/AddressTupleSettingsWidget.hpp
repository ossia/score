#pragma once
#include "AddressSettingsWidget.hpp"
#include <Device/Address/AddressSettings.hpp>

class QWidget;

class AddressTupleSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressTupleSettingsWidget(QWidget* parent = nullptr);

        iscore::AddressSettings getSettings() const override;

        void setSettings(const iscore::AddressSettings& settings) override;
};
