#pragma once
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

class QComboBox;
class QRadioButton;
class QWidget;

class MIDIProtocolSettingsWidget :
        public Device::ProtocolSettingsWidget
{
        Q_OBJECT

    public:
        MIDIProtocolSettingsWidget(QWidget* parent = nullptr);

        Device::DeviceSettings getSettings() const override;

        void setSettings(const Device::DeviceSettings& settings) override;

    protected slots:
        void updateInputDevices();
        void updateOutputDevices();

    protected:
        void buildGUI();

    protected:
        QRadioButton* m_inButton;
        QRadioButton* m_outButton;
        QComboBox* m_deviceCBox;

};

