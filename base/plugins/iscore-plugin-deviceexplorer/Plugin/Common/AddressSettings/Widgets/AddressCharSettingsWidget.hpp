#pragma once

#include "AddressSettingsWidget.hpp"
class AddressCharSettingsWidget : public AddressSettingsWidget
{
    public:
        explicit AddressCharSettingsWidget(QWidget* parent = nullptr);

        virtual iscore::AddressSettings getSettings() const override;
        virtual void setSettings(const iscore::AddressSettings& settings) override;

    protected:
        QLineEdit* m_valueEdit;

};

