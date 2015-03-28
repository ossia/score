#pragma once

class QLineEdit;
class QSpinBox;

#include <DeviceExplorer/Protocol/ProtocolSettingsWidget.hpp>

class MinuitProtocolSettingsWidget : public ProtocolSettingsWidget
{
    public:
        MinuitProtocolSettingsWidget(QWidget* parent = nullptr);

        virtual QList<QString> getSettings() const override;

        virtual void setSettings(const QList<QString>& settings) override;

    protected:
        void buildGUI();

        void setDefaults();

    protected:
        QLineEdit* m_deviceNameEdit;
        QSpinBox* m_portOutputSBox;
        QLineEdit* m_localHostEdit;

};

