#pragma once
#include <Device/Protocol/DeviceInterface.hpp>
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
        virtual ~OSSIADevice();
        void disconnect() override;
        bool connected() const override;

        void addAddress(const iscore::FullAddressSettings& settings) final override;
        void updateAddress(const iscore::FullAddressSettings& address) final override;
        void removeNode(const iscore::Address& path) final override;

        iscore::Node refresh() final override;

        // throws std::runtime_error
        boost::optional<iscore::Value> refresh(const iscore::Address&) final override;

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

        iscore::Node OSSIAToDeviceExplorer(const OSSIA::Node& node,
                                           iscore::Address currentAddr);

        std::shared_ptr<OSSIA::Device> m_dev;

        std::unordered_map<
            iscore::Address,
            std::pair<
                std::shared_ptr<OSSIA::Address>,
                OSSIA::CallbackContainer<OSSIA::ValueCallback>::iterator
            >
        > m_callbacks;

        bool m_connected = false;
};
