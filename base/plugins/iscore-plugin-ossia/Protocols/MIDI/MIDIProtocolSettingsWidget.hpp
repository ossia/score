#pragma once

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Device/Protocol/DeviceSettings.hpp>

class QComboBox;
class QRadioButton;
class QWidget;

class MIDIProtocolSettingsWidget : public ProtocolSettingsWidget
{
        Q_OBJECT

    public:
        MIDIProtocolSettingsWidget(QWidget* parent = nullptr);

        iscore::DeviceSettings getSettings() const override;

        void setSettings(const iscore::DeviceSettings& settings) override;

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

