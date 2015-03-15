#pragma once

#include "AddressSettingsWidget.hpp"

class QComboBox;
class QLineEdit;
class QSpinBox;


class AddressStringSettingsWidget : public AddressSettingsWidget
{
    public:
        AddressStringSettingsWidget(QWidget* parent = nullptr);

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
        QLineEdit* m_valueEdit;
        QSpinBox* m_prioritySBox;
        QLineEdit* m_tagsEdit;

};

