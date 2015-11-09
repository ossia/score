#pragma once
#include "Protocols/OSSIADevice.hpp"
#include "OSCSpecificSettings.hpp"
#include <API/Headers/Network/Protocol/OSC.h>

class OSCDevice final : public OSSIADevice
{
    public:
        OSCDevice(const iscore::DeviceSettings& stngs);

        bool reconnect() override;

        void updateOSSIASettings() override;

    private:
        std::shared_ptr<OSSIA::OSC> m_oscSettings;
};
