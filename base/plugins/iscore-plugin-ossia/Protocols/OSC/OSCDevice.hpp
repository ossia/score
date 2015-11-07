#pragma once
#include "Protocols/OSSIADevice.hpp"
#include "OSCSpecificSettings.hpp"
#include <API/Headers/Network/Protocol/OSC.h>

class OSCDevice final : public OSSIADevice
{
    public:
        OSCDevice(const iscore::DeviceSettings& stngs);
        void updateSettings(const iscore::DeviceSettings& settings) override;

        bool reconnect() override;


    private:
        std::shared_ptr<OSSIA::OSC> m_oscSettings;
};
