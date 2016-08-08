#pragma once
#include <Engine/Protocols/OSSIADevice.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <nano_observer.hpp>

namespace Engine
{
namespace Network
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

        ossia::net::device_base* getDevice() const override
        { return m_dev; }

    private:
        void disconnect() override;
        bool reconnect() override;

        void nodeCreated(const ossia::net::node_base&);
        void nodeRemoving(const ossia::net::node_base&);
        void nodeRenamed(const ossia::net::node_base&, std::string);
        Device::Node refresh() override;

        ossia::net::device_base* m_dev{};

        using OSSIADevice::refresh;
/*
          OSSIA::CallbackContainer<OSSIA::net::NodeChangeCallback>::iterator m_addedNodeCb;
        OSSIA::CallbackContainer<OSSIA::net::NodeChangeCallback>::iterator m_removedNodeCb;
        OSSIA::CallbackContainer<OSSIA::net::NodeChangeCallback>::iterator m_nameChangesCb;
        */
};
}
}
