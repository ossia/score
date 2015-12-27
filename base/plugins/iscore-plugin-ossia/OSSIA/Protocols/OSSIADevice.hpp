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

namespace iscore {
struct DeviceSettings;
struct FullAddressSettings;
struct Message;
struct Value;
}  // namespace iscore

namespace OSSIA
{
    class Device;
}

class OSSIADevice : public DeviceInterface
{
    public:
        virtual ~OSSIADevice();
        void disconnect() final override;
        bool connected() const final override;

        void updateSettings(const iscore::DeviceSettings& settings) final override;

        void addAddress(const iscore::FullAddressSettings& settings) final override;
        void updateAddress(const iscore::FullAddressSettings& address) final override;
        void removeNode(const iscore::Address& path) final override;

        iscore::Node refresh() override;

        // throws std::runtime_error
        boost::optional<iscore::Value> refresh(const iscore::Address&) final override;

        iscore::Node getNode(const iscore::FullAddressSettings&) final override;

        void setListening(const iscore::Address&, bool) final override;

        std::vector<iscore::Address> listening() const final override;
        void replaceListening(const std::vector<iscore::Address>&) final override;

        void sendMessage(const iscore::Message& mess) final override;
        bool check(const QString& str) final override;

        OSSIA::Device& impl() const;
        std::shared_ptr<OSSIA::Device> impl_ptr() const
        { return m_dev; }

    protected:
        using DeviceInterface::DeviceInterface;

        std::shared_ptr<OSSIA::Device> m_dev;

        std::unordered_map<
            iscore::Address,
            std::pair<
                std::shared_ptr<OSSIA::Address>,
                OSSIA::CallbackContainer<OSSIA::ValueCallback>::iterator
            >
        > m_callbacks;

    private:
        void removeListening_impl(OSSIA::Node &node, iscore::Address addr);
};
