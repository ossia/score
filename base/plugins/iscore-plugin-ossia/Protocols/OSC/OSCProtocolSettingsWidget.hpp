#pragma once

class QLineEdit;
class QSpinBox;

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

class OSCProtocolSettingsWidget : public ProtocolSettingsWidget
{
        Q_OBJECT

    public:
        OSCProtocolSettingsWidget(QWidget* parent = nullptr);

        virtual iscore::DeviceSettings getSettings() const override;
        virtual QString getPath() const override;

        virtual void setSettings(const iscore::DeviceSettings& settings) override;

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

