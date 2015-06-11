#pragma once

#include "AddressSettingsWidget.hpp"

class QComboBox;
class QLineEdit;
class QSpinBox;
struct AddressSettings;

class AddressStringSettingsWidget : public AddressSettingsWidget
{
    public:
        AddressStringSettingsWidget(QWidget* parent = nullptr);

        virtual AddressSettings getSettings() const override;
        virtual void setSettings(const AddressSettings& settings) override;

    protected:
        QLineEdit* m_valueEdit;

};

