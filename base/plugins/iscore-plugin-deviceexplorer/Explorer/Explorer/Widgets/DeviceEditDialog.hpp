#pragma once

class QComboBox;
class QGridLayout;
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <QDialog>
#include <QList>
#include <QString>


class DeviceEditDialog : public QDialog
{
        Q_OBJECT

    public:
        explicit DeviceEditDialog(QWidget* parent);
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

        QComboBox* m_protocolCBox;
        ProtocolSettingsWidget* m_protocolWidget;
        QGridLayout* m_gLayout;
        QList<iscore::DeviceSettings> m_previousSettings;
        int m_index;

        bool m_invalidState{false};
};

