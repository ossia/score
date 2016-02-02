#pragma once

#include "AddressSettingsWidget.hpp"
#include <Device/Address/AddressSettings.hpp>

class QLineEdit;
class QWidget;

namespace Explorer
{
class AddressCharSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressCharSettingsWidget(QWidget* parent = nullptr);

        Device::AddressSettings getSettings() const override;
        void setSettings(const Device::AddressSettings& settings) override;

    protected:
        QLineEdit* m_valueEdit;

};
}
