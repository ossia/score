#pragma once

#include <QDialog>
#include <QList>
#include <QString>

#include <Device/Protocol/DeviceSettings.hpp>

class QComboBox;
class QGridLayout;
class QWidget;

namespace Device
{
class DynamicProtocolList;
class ProtocolSettingsWidget;
}
namespace Explorer
{
class DeviceEditDialog final : public QDialog
{
        Q_OBJECT

    public:
        explicit DeviceEditDialog(
                const Device::DynamicProtocolList& pl,
                QWidget* parent);
        ~DeviceEditDialog();

        Device::DeviceSettings getSettings() const;
        QString getPath() const;

        void setSettings(const Device::DeviceSettings& settings);

        // This mode will display a warning to
        // the user if he has to edit the device again.
        void setEditingInvalidState(bool);


    protected slots:

        void updateProtocolWidget();

    protected:

        void buildGUI();

        void initAvailableProtocols();

    protected:
        const Device::DynamicProtocolList& m_protocolList;

        QComboBox* m_protocolCBox;
        Device::ProtocolSettingsWidget* m_protocolWidget;
        QGridLayout* m_gLayout;
        QList<Device::DeviceSettings> m_previousSettings;
        int m_index;

        bool m_invalidState{false};
};
}

