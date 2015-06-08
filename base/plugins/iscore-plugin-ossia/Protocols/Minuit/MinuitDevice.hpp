#pragma once
#include "Protocols/OSSIADevice.hpp"
#include "MinuitSpecificSettings.hpp"
#include <API/Headers/Network/Protocol.h>
namespace OSSIA
{
class Device;
}
class MinuitDevice : public OSSIADevice
{
    public:
        MinuitDevice(const DeviceSettings& settings);
        bool canRefresh() const override;
        Node refresh() override;

    private:
        OSSIA::Minuit m_minuitSettings;
};
