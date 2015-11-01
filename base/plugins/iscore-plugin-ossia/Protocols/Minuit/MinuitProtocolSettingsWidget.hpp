#pragma once

class QLineEdit;
class QSpinBox;

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

class MinuitProtocolSettingsWidget : public ProtocolSettingsWidget
{
    public:
        MinuitProtocolSettingsWidget(QWidget* parent = nullptr);

        virtual iscore::DeviceSettings getSettings() const override;

        virtual void setSettings(const iscore::DeviceSettings& settings) override;

    protected:
        void buildGUI();

        void setDefaults();

    protected:
        QLineEdit* m_deviceNameEdit;
        QSpinBox* m_portInputSBox;
        QSpinBox* m_portOutputSBox;
        QLineEdit* m_localHostEdit;

};

