#pragma once

class QComboBox;
class QGridLayout;
class QLineEdit;
class AddressSettingsWidget;
class AddressSettings;

#include <QDialog>
#include <QList>
#include <QString>


class AddressEditDialog : public QDialog
{
        Q_OBJECT

    public:

        AddressEditDialog(QWidget* parent);
        ~AddressEditDialog();

        // TODO
        AddressSettings getSettings() const;

        void setSettings(AddressSettings &settings);

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


