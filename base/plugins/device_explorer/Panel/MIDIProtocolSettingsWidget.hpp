#pragma once

class QRadioButton;
class QComboBox;

#include "ProtocolSettingsWidget.hpp"

class MIDIProtocolSettingsWidget : public ProtocolSettingsWidget
{
        Q_OBJECT

    public:
        MIDIProtocolSettingsWidget (QWidget* parent = nullptr);

        virtual QList<QString> getSettings() const override;

        virtual void setSettings (const QList<QString>& settings) override;

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

