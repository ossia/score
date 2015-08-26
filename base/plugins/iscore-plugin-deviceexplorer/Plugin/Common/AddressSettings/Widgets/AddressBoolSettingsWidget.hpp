#pragma once
#include "AddressSettingsWidget.hpp"

class AddressBoolSettingsWidget : public AddressSettingsWidget
{
    public:
        explicit AddressBoolSettingsWidget(QWidget* parent = nullptr);

        virtual iscore::AddressSettings getSettings() const override;

        virtual void setSettings(const iscore::AddressSettings& settings) override;

    private:
        QComboBox* m_cb{};
};
