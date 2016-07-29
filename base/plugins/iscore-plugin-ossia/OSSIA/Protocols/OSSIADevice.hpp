#pragma once
#include <Device/Protocol/DeviceInterface.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <ossia/detail/callback_container.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <State/Address.hpp>
#include <iscore_plugin_ossia_export.h>

#include <QString>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include <functional>
namespace ossia
{
class value;
namespace net
{
class address;
class Node;
class Device;
using ValueCallback = std::function<void(const ossia::value&)>;
}
}

namespace Ossia
{
namespace Protocols
{

class ISCORE_PLUGIN_OSSIA_EXPORT OSSIADevice :
        public Device::DeviceInterface
{
    public:
        virtual ~OSSIADevice();
        void disconnect() override;
        bool connected() const final override;

        void updateSettings(const Device::DeviceSettings& settings) final override;

        void addAddress(const Device::FullAddressSettings& settings) final override;
        void updateAddress(
                const State::Address& currentAddr,
                const Device::FullAddressSettings& address) final override;
        void removeNode(const State::Address& path) final override;

        Device::Node refresh() override;

        // throws std::runtime_error
        optional<State::Value> refresh(const State::Address&) final override;

        Device::Node getNode(const State::Address&) final override;

        void setListening(const State::Address&, bool) final override;

        std::vector<State::Address> listening() const final override;
        void addToListening(const std::vector<State::Address>&) final override;

        void sendMessage(const State::Message& mess) final override;

        bool isLogging() const final override;
        void setLogging(bool) final override;

        virtual ossia::net::Device* getDevice() const = 0;

    protected:
        using DeviceInterface::DeviceInterface;

        std::unordered_map<
            State::Address,
            std::pair<
                ossia::net::address*,
                ossia::callback_container<ossia::net::ValueCallback>::iterator
            >
        > m_callbacks;

        void removeListening_impl(ossia::net::Node &node, State::Address addr);
        void setLogging_impl(bool) const;
    private:
        bool m_logging = false;
};

class ISCORE_PLUGIN_OSSIA_EXPORT OwningOSSIADevice :
        public OSSIADevice
{
    protected:
        void disconnect() override;

        using OSSIADevice::OSSIADevice;

        ossia::net::Device* getDevice() const final override
        { return m_dev.get(); }

        std::unique_ptr<ossia::net::Device> m_dev;
};
}
}
