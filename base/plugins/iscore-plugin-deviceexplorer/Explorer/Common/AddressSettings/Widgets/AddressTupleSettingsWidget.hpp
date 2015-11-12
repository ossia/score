#pragma once
#include "AddressSettingsWidget.hpp"

class AddressTupleSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressTupleSettingsWidget(QWidget* parent = nullptr);

        iscore::AddressSettings getSettings() const override;

        void setSettings(const iscore::AddressSettings& settings) override;
};
