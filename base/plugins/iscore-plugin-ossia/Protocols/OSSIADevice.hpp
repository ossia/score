#pragma once
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>
#include <Network/Address.h>
#include <unordered_map>
namespace OSSIA
{
    class Device;
    class Domain;
    class Node;
}

class OSSIADevice : public DeviceInterface
{
    public:
        void addAddress(const iscore::FullAddressSettings& settings) override;
        void updateAddress(const iscore::FullAddressSettings& address) override;
        void removeNode(const iscore::Address& path) override;

        iscore::Node refresh() override;

        // throws std::runtime_error
        boost::optional<iscore::Value> refresh(const iscore::Address&) override;

        void setListening(const iscore::Address&, bool) override;

        void sendMessage(iscore::Message mess) override;
        bool check(const QString& str) override;

        OSSIA::Device& impl() const;
        std::shared_ptr<OSSIA::Device> impl_ptr() const
        { return m_dev; }

    protected:
        using DeviceInterface::DeviceInterface;

        iscore::Node OSSIAToDeviceExplorer(const OSSIA::Node& node,
                                           iscore::Address currentAddr);

        std::shared_ptr<OSSIA::Device> m_dev;

        std::unordered_map<iscore::Address, OSSIA::CallbackContainer<OSSIA::ValueCallback>::iterator> m_callbacks;
};
