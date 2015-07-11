#pragma once
#include "AddressSettingsWidget.hpp"

class AddressNoneSettingsWidget : public AddressSettingsWidget
{
    public:
        AddressNoneSettingsWidget(QWidget* parent = nullptr);

        virtual AddressSettings getSettings() const override;

        virtual void setSettings(const AddressSettings& settings) override;
};
