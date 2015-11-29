#pragma once

#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <qstring.h>

#include "Device/Protocol/DeviceSettings.hpp"

class QLineEdit;
class QSpinBox;
class QWidget;

class OSCProtocolSettingsWidget : public ProtocolSettingsWidget
{
        Q_OBJECT

    public:
        OSCProtocolSettingsWidget(QWidget* parent = nullptr);

        iscore::DeviceSettings getSettings() const override;
        QString getPath() const override;

        void setSettings(const iscore::DeviceSettings& settings) override;

    protected slots:
        void openFileDialog();

    protected:
        void buildGUI();

        void setDefaults();

    protected:
        QLineEdit* m_deviceNameEdit;
        QSpinBox* m_portOutputSBox;
        QSpinBox* m_portInputSBox;
        QLineEdit* m_localHostEdit;
        QLineEdit* m_namespaceFilePathEdit;
};

