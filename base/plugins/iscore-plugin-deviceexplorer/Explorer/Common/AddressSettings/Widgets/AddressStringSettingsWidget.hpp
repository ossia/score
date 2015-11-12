#pragma once

#include "AddressSettingsWidget.hpp"

class QComboBox;
class QLineEdit;
class QSpinBox;
class AddressStringSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressStringSettingsWidget(QWidget* parent = nullptr);

        iscore::AddressSettings getSettings() const override;
        void setSettings(const iscore::AddressSettings& settings) override;

    protected:
        QLineEdit* m_valueEdit;

};
