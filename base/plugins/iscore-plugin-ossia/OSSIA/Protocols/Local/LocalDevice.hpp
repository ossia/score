#pragma once
#include <OSSIA/Protocols/OSSIADevice.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <nano_observer.hpp>

namespace Ossia
{
namespace Protocols
{
class LocalDevice final :
        public OSSIADevice,
        public Nano::Observer
{
    public:
        LocalDevice(
                const iscore::DocumentContext& ctx,
                const Device::DeviceSettings& settings);

        ~LocalDevice();

        ossia::net::Device* getDevice() const override
        { return m_dev; }

    private:
        void disconnect() override;
        bool reconnect() override;

        void nodeCreated(const ossia::net::Node&);
        void nodeRemoving(const ossia::net::Node&);
        void nodeRenamed(const ossia::net::Node&, std::string);
        Device::Node refresh() override;

        ossia::net::Device* m_dev{};

        using OSSIADevice::refresh;
/*
          OSSIA::CallbackContainer<OSSIA::net::NodeChangeCallback>::iterator m_addedNodeCb;
        OSSIA::CallbackContainer<OSSIA::net::NodeChangeCallback>::iterator m_removedNodeCb;
        OSSIA::CallbackContainer<OSSIA::net::NodeChangeCallback>::iterator m_nameChangesCb;
        */
};
}
}
