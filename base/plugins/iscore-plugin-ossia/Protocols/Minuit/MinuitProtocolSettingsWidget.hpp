#pragma once

class QLineEdit;
class QSpinBox;

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

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

