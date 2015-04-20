#pragma once

#include "AddressSettingsWidget.hpp"

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;
struct AddressSettings;

class AddressFloatSettingsWidget : public AddressSettingsWidget
{
    public:
        AddressFloatSettingsWidget(QWidget* parent = nullptr);

        //TODO: use QVariant instead ???
        virtual AddressSettings getSettings() const override;

        virtual void setSettings(const AddressSettings& settings) override;

    protected:

        void buildGUI();

        void setDefaults();

    protected:

        QComboBox* m_ioTypeCBox;
        QDoubleSpinBox* m_valueSBox;
        QDoubleSpinBox* m_minSBox;
        QDoubleSpinBox* m_maxSBox;
        QComboBox* m_unitCBox;
        QComboBox* m_clipModeCBox;
        QSpinBox* m_prioritySBox;
        QLineEdit* m_tagsEdit;

};

