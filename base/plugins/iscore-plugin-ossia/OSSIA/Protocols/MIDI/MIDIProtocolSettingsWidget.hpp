#pragma once
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <ossia/network/v1/Protocol/MIDI.hpp>


class QComboBox;
class QCheckBox;
class QRadioButton;
class QWidget;
class QLineEdit;

namespace Ossia
{
class MIDIProtocolSettingsWidget :
        public Device::ProtocolSettingsWidget
{
        Q_OBJECT

    public:
        MIDIProtocolSettingsWidget(QWidget* parent = nullptr);


    private:
        Device::DeviceSettings getSettings() const override;

        void setSettings(const Device::DeviceSettings& settings) override;

        void updateDevices(OSSIA::MidiInfo::Type);
        void updateInputDevices();
        void updateOutputDevices();

        void buildGUI();

        QLineEdit* m_name;
        QCheckBox* m_inButton;
        QCheckBox* m_outButton;
        QComboBox* m_deviceCBox;

};
}
