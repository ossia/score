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
namespace OSSIA
{
class Value;
namespace v2
{
class Address;
class Node;
class Device;
using ValueCallback = std::function<void(const OSSIA::Value&)>;
}
}

namespace Ossia
{
namespace Protocols
{

class ISCORE_PLUGIN_OSSIA_EXPORT OSSIADevice_v2 :
        public Device::DeviceInterface
{
    public:
        virtual ~OSSIADevice_v2();
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

        OSSIA::v2::Device& impl() const;

    protected:
        using DeviceInterface::DeviceInterface;

        std::unique_ptr<OSSIA::v2::Device> m_dev;

        std::unordered_map<
            State::Address,
            std::pair<
                OSSIA::v2::Address*,
                OSSIA::CallbackContainer<OSSIA::v2::ValueCallback>::iterator
            >
        > m_callbacks;

        void removeListening_impl(OSSIA::v2::Node &node, State::Address addr);
        void setLogging_impl(bool) const;
    private:
        bool m_logging = false;
};
}
}
