#pragma once

#include "AddressSettingsWidget.hpp"

class QComboBox;
class QLineEdit;
class QSpinBox;
class AddressStringSettingsWidget : public AddressSettingsWidget
{
    public:
        AddressStringSettingsWidget(QWidget* parent = nullptr);

        virtual iscore::AddressSettings getSettings() const override;
        virtual void setSettings(const iscore::AddressSettings& settings) override;

    protected:
        QLineEdit* m_valueEdit;

};

