#pragma once

class QComboBox;
class QGridLayout;
#include <DeviceExplorer/Protocol/ProtocolSettingsWidget.hpp>

#include <QDialog>
#include <QList>
#include <QString>


class DeviceEditDialog : public QDialog
{
        Q_OBJECT

    public:

        DeviceEditDialog(QWidget* parent);
        ~DeviceEditDialog();

        DeviceSettings getSettings() const;

        void setSettings(DeviceSettings& settings);


    protected slots:

        void updateProtocolWidget();

    protected:

        void buildGUI();

        void initAvailableProtocols();

    protected:

        QComboBox* m_protocolCBox;
        ProtocolSettingsWidget* m_protocolWidget;
        QGridLayout* m_gLayout;
        QList<DeviceSettings> m_previousSettings;
        int m_index;
};

