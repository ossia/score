#pragma once
#include "AddressSettingsWidget.hpp"

class AddressTupleSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressTupleSettingsWidget(QWidget* parent = nullptr);

        virtual iscore::AddressSettings getSettings() const override;

        virtual void setSettings(const iscore::AddressSettings& settings) override;
};
