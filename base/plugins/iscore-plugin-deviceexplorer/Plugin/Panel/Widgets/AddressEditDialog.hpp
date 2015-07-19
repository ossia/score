#pragma once
#include <QDialog>
#include <QList>
#include <QString>
#include <DeviceExplorer/Address/AddressSettings.hpp>

class QComboBox;
class QGridLayout;
class QLineEdit;
class AddressSettingsWidget;
class AddressEditDialog : public QDialog
{
        Q_OBJECT

    public:
        AddressEditDialog(QWidget* parent);
        ~AddressEditDialog();

        iscore::AddressSettings getSettings() const;
        static iscore::AddressSettings makeDefaultSettings();

        void setSettings(const iscore::AddressSettings& settings);

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
        QList<iscore::AddressSettings> m_previousSettings;
        int m_index;
};


