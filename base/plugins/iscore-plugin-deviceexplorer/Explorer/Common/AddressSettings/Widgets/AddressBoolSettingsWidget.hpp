#pragma once
#include "AddressSettingsWidget.hpp"

class AddressBoolSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressBoolSettingsWidget(QWidget* parent = nullptr);

        iscore::AddressSettings getSettings() const override;

        void setSettings(const iscore::AddressSettings& settings) override;

    private:
        QComboBox* m_cb{};
};
