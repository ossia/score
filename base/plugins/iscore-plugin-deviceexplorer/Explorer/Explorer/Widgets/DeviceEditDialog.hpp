#pragma once

#include <QDialog>
#include <QList>
#include <QString>

#include "Device/Protocol/DeviceSettings.hpp"

class DynamicProtocolList;
class ProtocolSettingsWidget;
class QComboBox;
class QGridLayout;
class QWidget;


class DeviceEditDialog final : public QDialog
{
        Q_OBJECT

    public:
        explicit DeviceEditDialog(
                const DynamicProtocolList& pl,
                QWidget* parent);
        ~DeviceEditDialog();

        iscore::DeviceSettings getSettings() const;
        QString getPath() const;

        void setSettings(const iscore::DeviceSettings& settings);

        // This mode will display a warning to
        // the user if he has to edit the device again.
        void setEditingInvalidState(bool);


    protected slots:

        void updateProtocolWidget();

    protected:

        void buildGUI();

        void initAvailableProtocols();

    protected:
        const DynamicProtocolList& m_protocolList;

        QComboBox* m_protocolCBox;
        ProtocolSettingsWidget* m_protocolWidget;
        QGridLayout* m_gLayout;
        QList<iscore::DeviceSettings> m_previousSettings;
        int m_index;

        bool m_invalidState{false};
};

