#pragma once
#include "AddressSettingsWidget.hpp"
#include <Common/CommonTypes.hpp>

class AddressBoolSettingsWidget : public AddressSettingsWidget
{
    public:
        AddressBoolSettingsWidget(QWidget* parent = nullptr);

        virtual AddressSettings getSettings() const override;

        virtual void setSettings(const AddressSettings& settings) override;

    private:
        QComboBox* m_cb{};
};
