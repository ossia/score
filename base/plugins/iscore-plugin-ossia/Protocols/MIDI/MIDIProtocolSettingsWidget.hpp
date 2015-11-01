#pragma once

class QRadioButton;
class QComboBox;

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

class MIDIProtocolSettingsWidget : public ProtocolSettingsWidget
{
        Q_OBJECT

    public:
        MIDIProtocolSettingsWidget(QWidget* parent = nullptr);

        virtual iscore::DeviceSettings getSettings() const override;

        virtual void setSettings(const iscore::DeviceSettings& settings) override;

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

