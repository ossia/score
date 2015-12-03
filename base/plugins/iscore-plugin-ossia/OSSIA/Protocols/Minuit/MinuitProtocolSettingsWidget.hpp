#pragma once

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Device/Protocol/DeviceSettings.hpp>

class QLineEdit;
class QSpinBox;
class QWidget;

class MinuitProtocolSettingsWidget : public ProtocolSettingsWidget
{
    public:
        MinuitProtocolSettingsWidget(QWidget* parent = nullptr);

        iscore::DeviceSettings getSettings() const override;

        void setSettings(const iscore::DeviceSettings& settings) override;

    protected:
        void buildGUI();

        void setDefaults();

    protected:
        QLineEdit* m_deviceNameEdit;
        QSpinBox* m_portInputSBox;
        QSpinBox* m_portOutputSBox;
        QLineEdit* m_localHostEdit;

};

