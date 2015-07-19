#pragma once
#include "Protocols/OSSIADevice.hpp"
#include "OSCSpecificSettings.hpp"

class OSCDevice : public OSSIADevice
{
        //std::shared_ptr<OSSIA::Device> m_device{};
    public:
        OSCDevice(const iscore::DeviceSettings& stngs);
};
