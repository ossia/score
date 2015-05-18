#pragma once

#include "AddressSettingsWidget.hpp"

class QComboBox;
class QLineEdit;
class QSpinBox;
struct AddressSettings;

class AddressIntSettingsWidget : public AddressSettingsWidget
{
    public:
        AddressIntSettingsWidget(QWidget* parent = nullptr);

        virtual AddressSettings getSettings() const override;
        virtual void setSettings(const AddressSettings& settings) override;

    protected:

        void buildGUI();

        void setDefaults();

    protected:

        QComboBox* m_ioTypeCBox;
        QSpinBox* m_valueSBox;
        QSpinBox* m_minSBox;
        QSpinBox* m_maxSBox;
        QComboBox* m_unitCBox;
        QComboBox* m_clipModeCBox;
        QSpinBox* m_prioritySBox;
        QLineEdit* m_tagsEdit;

};

