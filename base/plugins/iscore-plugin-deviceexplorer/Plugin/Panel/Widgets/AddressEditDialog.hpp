#pragma once

class QComboBox;
class QGridLayout;
class QLineEdit;
class AddressSettingsWidget;

#include <QDialog>
#include <QList>
#include <QString>
#include "Common/AddressSettings/AddressSettings.hpp"


class AddressEditDialog : public QDialog
{
        Q_OBJECT

    public:
        AddressEditDialog(QWidget* parent);
        ~AddressEditDialog();

        AddressSettings getSettings() const;
        static AddressSettings makeDefaultSettings();

        void setSettings(const AddressSettings& settings);

    protected slots:

        void updateNodeWidget();

    protected:

        void buildGUI();

        void initAvailableValueTypes();

    protected:
        QLineEdit* m_nameEdit;
        QComboBox* m_valueTypeCBox;
        AddressSettingsWidget* m_addressWidget;
        QGridLayout* m_gLayout;
        QList<AddressSettings> m_previousSettings;
        int m_index;
};


