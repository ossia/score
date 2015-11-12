#pragma once
#include "AddressSettingsWidget.hpp"

class AddressNoneSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressNoneSettingsWidget(QWidget* parent = nullptr);

        iscore::AddressSettings getSettings() const override;

        void setSettings(const iscore::AddressSettings& settings) override;
};
