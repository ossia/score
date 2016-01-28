#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <Explorer/Widgets/ValueWrapper.hpp>
#include <QDialog>

class QComboBox;
class QFormLayout;
class QLineEdit;
class QWidget;

namespace DeviceExplorer
{

class AddressSettingsWidget;
class AddressEditDialog final : public QDialog
{
        Q_OBJECT

    public:
        // Creation of an address
        explicit AddressEditDialog(
                QWidget* parent);

        // Edition of an address
        explicit AddressEditDialog(
                const Device::AddressSettings& addr,
                QWidget* parent);
        ~AddressEditDialog();

        Device::AddressSettings getSettings() const;
        static Device::AddressSettings makeDefaultSettings();

    protected:
        void setNodeSettings();
        void setValueSettings();
        void updateType();

        Device::AddressSettings m_originalSettings;
        QLineEdit* m_nameEdit{};
        QComboBox* m_valueTypeCBox{};
        WidgetWrapper<AddressSettingsWidget>* m_addressWidget{};
        QFormLayout* m_layout{};
};
}
