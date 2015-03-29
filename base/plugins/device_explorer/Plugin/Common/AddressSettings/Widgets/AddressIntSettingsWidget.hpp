#pragma once

#include "AddressSettingsWidget.hpp"

class QComboBox;
class QLineEdit;
class QSpinBox;


class AddressIntSettingsWidget : public AddressSettingsWidget
{
    public:
        AddressIntSettingsWidget(QWidget* parent = nullptr);

        //TODO: use QVariant instead ???
        /*

          The first element in returned list must be the name of the device.
         */
        virtual QList<QString> getSettings() const override;

        virtual void setSettings(const QList<QString>& settings) override;

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

