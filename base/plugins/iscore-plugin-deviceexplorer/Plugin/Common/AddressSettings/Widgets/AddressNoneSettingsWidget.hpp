#pragma once
#include "AddressSettingsWidget.hpp"

class AddressNoneSettingsWidget : public AddressSettingsWidget
{
    public:
        AddressNoneSettingsWidget(QWidget* parent = nullptr);

        virtual iscore::AddressSettings getSettings() const override;

        virtual void setSettings(const iscore::AddressSettings& settings) override;
};
