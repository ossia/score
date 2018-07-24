#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <wobjectdefs.h>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

class QLineEdit;
class QComboBox;

namespace Engine {

    namespace Network {

        class JoystickProtocolSettingsWidget final : 
            public Device::ProtocolSettingsWidget 
        {
            W_OBJECT(JoystickProtocolSettingsWidget)

            public:
                JoystickProtocolSettingsWidget(QWidget *parent = nullptr);
                virtual ~JoystickProtocolSettingsWidget();
                Device::DeviceSettings getSettings() const override;
                void setSettings(const Device::DeviceSettings& settings) override;

            public:
                void update_device_list();

            protected:
                QLineEdit *m_deviceNameEdit;
                QComboBox *m_deviceChoice;

        };
    }
}
