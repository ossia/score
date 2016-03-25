#pragma once
#include <Device/Protocol/DeviceInterface.hpp>
#include <Network/Address.h>
#include <boost/optional/optional.hpp>
#include <QString>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Device/Node/DeviceNode.hpp>
#include "Misc/CallbackContainer.h"
#include <State/Address.hpp>
#include <iscore_plugin_ossia_export.h>

namespace Device {
struct DeviceSettings;
struct FullAddressSettings;
}
namespace State
{
struct Message;
struct Value;
}  // namespace iscore

namespace OSSIA
{
    class Device;
}

namespace Ossia
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
        boost::optional<State::Value> refresh(const State::Address&) final override;

        Device::Node getNode(const State::Address&) final override;

        void setListening(const State::Address&, bool) final override;

        std::vector<State::Address> listening() const final override;
        void addToListening(const std::vector<State::Address>&) final override;

        void sendMessage(const State::Message& mess) final override;
        bool check(const QString& str) final override;

        OSSIA::Device& impl() const;
        std::shared_ptr<OSSIA::Device> impl_ptr() const
        { return m_dev; }

    protected:
        using DeviceInterface::DeviceInterface;

        std::shared_ptr<OSSIA::Device> m_dev;

        std::unordered_map<
            State::Address,
            std::pair<
                std::shared_ptr<OSSIA::Address>,
                OSSIA::CallbackContainer<OSSIA::ValueCallback>::iterator
            >
        > m_callbacks;

    private:
        void removeListening_impl(OSSIA::Node &node, State::Address addr);
};
}
