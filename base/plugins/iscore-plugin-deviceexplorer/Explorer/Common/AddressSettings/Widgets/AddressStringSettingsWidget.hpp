#pragma once

#include "AddressSettingsWidget.hpp"
#include <Device/Address/AddressSettings.hpp>

class QLineEdit;
class QWidget;

namespace DeviceExplorer
{
class AddressStringSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressStringSettingsWidget(QWidget* parent = nullptr);

        Device::AddressSettings getSettings() const override;
        void setSettings(const Device::AddressSettings& settings) override;

    protected:
        QLineEdit* m_valueEdit;

};
}
