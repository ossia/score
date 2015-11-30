#pragma once

#include "AddressSettingsWidget.hpp"
#include <Device/Address/AddressSettings.hpp>

class QLineEdit;
class QWidget;

class AddressStringSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressStringSettingsWidget(QWidget* parent = nullptr);

        iscore::AddressSettings getSettings() const override;
        void setSettings(const iscore::AddressSettings& settings) override;

    protected:
        QLineEdit* m_valueEdit;

};
