#pragma once
#include "AddressSettingsWidget.hpp"

class AddressTupleSettingsWidget : public AddressSettingsWidget
{
    public:
        AddressTupleSettingsWidget(QWidget* parent = nullptr);

        virtual AddressSettings getSettings() const override;

        virtual void setSettings(const AddressSettings& settings) override;
};
