#pragma once
#include <OSSIA/Protocols/OSSIADevice.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>

namespace Ossia
{
namespace Protocols
{
class LocalDevice final : public OSSIADevice
{
    public:
        LocalDevice(
                const iscore::DocumentContext& ctx,
                const Device::DeviceSettings& settings);

        ~LocalDevice();

    private:
        void disconnect() override;
        bool reconnect() override;

        Device::Node refresh() override;


        using OSSIADevice::refresh;
/*
          OSSIA::CallbackContainer<OSSIA::net::NodeChangeCallback>::iterator m_addedNodeCb;
        OSSIA::CallbackContainer<OSSIA::net::NodeChangeCallback>::iterator m_removedNodeCb;
        OSSIA::CallbackContainer<OSSIA::net::NodeChangeCallback>::iterator m_nameChangesCb;
        */
};
}
}
