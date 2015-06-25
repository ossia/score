#pragma once
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>
#include <API/Headers/Network/Protocol.h>
#include <API/Headers/Network/Device.h>
#include <API/Headers/Network/Address.h>
#include <API/Headers/Editor/Value.h>
#include <API/Headers/Editor/Domain.h>

namespace OSSIA
{
    class Device;
    class Domain;
}

class OSSIADevice : public DeviceInterface
{
    public:
        using DeviceInterface::DeviceInterface;

        void addAddress(const FullAddressSettings& settings) override;
        void updateAddress(const FullAddressSettings& address) override;
        void removeAddress(const QString& path) override;

        void sendMessage(iscore::Message& mess) override;
        bool check(const QString& str) override;

    protected:
        std::shared_ptr<OSSIA::Device> m_dev;
};


// Gets a node from an address in a device.
// Creates it if necessary.

OSSIA::Node* getNodeFromPath(
        const QStringList& path,
        OSSIA::Device *dev);
OSSIA::Node* createNodeFromPath(
        const QStringList& path,
        OSSIA::Device* dev);
void createOSSIAAddress(
        const FullAddressSettings& settings,
        OSSIA::Node* node);
void updateOSSIAAddress(
        const FullAddressSettings& settings,
        const std::shared_ptr<OSSIA::Address>& addr);
void updateOSSIAValue(
        const QVariant& data,
        OSSIA::Value& val);

// Utility functions to convert from one node to another.
IOType OSSIAAccessModeToIOType(OSSIA::Address::AccessMode t);
ClipMode OSSIABoudingModeToClipMode(OSSIA::Address::BoundingMode b);
Domain OSSIADomainToDomain(OSSIA::Domain& domain);
QVariant OSSIAValueToVariant(const OSSIA::Value* val);
AddressSettings extractAddressSettings(const OSSIA::Node& node);
Node* OssiaToDeviceExplorer(const OSSIA::Node& node);


