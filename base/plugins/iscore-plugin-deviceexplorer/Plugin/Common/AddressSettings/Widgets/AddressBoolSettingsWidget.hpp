#pragma once
#include "AddressSettingsWidget.hpp"

class AddressBoolSettingsWidget : public AddressSettingsWidget
{
    public:
        AddressBoolSettingsWidget(QWidget* parent = nullptr);

        virtual AddressSettings getSettings() const override;

        virtual void setSettings(const AddressSettings& settings) override;

    private:
        QComboBox* m_cb{};
};



// TODO refactor in own file
class AddressNoneSettingsWidget : public AddressSettingsWidget
{
    public:
        AddressNoneSettingsWidget(QWidget* parent = nullptr);

        virtual AddressSettings getSettings() const override;

        virtual void setSettings(const AddressSettings& settings) override;

    private:
        QComboBox* m_cb{};
};
