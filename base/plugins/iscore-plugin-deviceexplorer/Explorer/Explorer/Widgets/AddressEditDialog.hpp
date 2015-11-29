#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <Explorer/Widgets/ValueWrapper.hpp>
#include <QDialog>

class AddressSettingsWidget;
class QComboBox;
class QFormLayout;
class QLineEdit;
class QWidget;

class AddressEditDialog final : public QDialog
{
        Q_OBJECT

    public:
        // Creation of an address
        explicit AddressEditDialog(
                QWidget* parent);

        // Edition of an address
        explicit AddressEditDialog(
                const iscore::AddressSettings& addr,
                QWidget* parent);
        ~AddressEditDialog();

        iscore::AddressSettings getSettings() const;
        static iscore::AddressSettings makeDefaultSettings();

    protected:
        void setNodeSettings();
        void setValueSettings();
        void updateType();

        iscore::AddressSettings m_originalSettings;
        QLineEdit* m_nameEdit{};
        QComboBox* m_valueTypeCBox{};
        WidgetWrapper<AddressSettingsWidget>* m_addressWidget{};
        QFormLayout* m_layout{};
};


