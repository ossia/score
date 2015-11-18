#pragma once

#include "AddressSettingsWidget.hpp"
class AddressCharSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressCharSettingsWidget(QWidget* parent = nullptr);

        iscore::AddressSettings getSettings() const override;
        void setSettings(const iscore::AddressSettings& settings) override;

    protected:
        QLineEdit* m_valueEdit;

};

