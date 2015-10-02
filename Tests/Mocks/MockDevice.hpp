#pragma once
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>
#include <Singletons/SingletonProtocolList.hpp>
#include <DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp>
#include <DeviceExplorer/Protocol/ProtocolSettingsWidget.hpp>

class MockDevice : public DeviceInterface
{
    public:
        MockDevice(const iscore::DeviceSettings& set):
            DeviceInterface(set)
        {

        }

        void updateSettings(const iscore::DeviceSettings&)
        {
        }

        bool canRefresh() const
        {
            return false;
        }

        void setListening(const iscore::Address&, bool)
        {
        }

        void replaceListening(const std::vector<iscore::Address>&)
        {
        }

        std::vector<iscore::Address> listening() const
        {
            return {};
        }

        void addAddress(const iscore::FullAddressSettings& addr)
        {
            m_addresses.insert({addr.address, addr});
        }

        void updateAddress(const iscore::FullAddressSettings& addr)
        {
            m_addresses.insert({addr.address, addr});
        }

        void removeNode(const iscore::Address& addr)
        {
            m_addresses.erase(addr);
        }

        void sendMessage(iscore::Message mess)
        {
        }

        bool check(const QString& str)
        {
            return false;
        }

        std::unordered_map<iscore::Address, iscore::FullAddressSettings> m_addresses;
};

class MockDeviceSettings : public ProtocolSettingsWidget
{
    public:
        iscore::DeviceSettings getSettings() const
        {
            return {};
        }

        QString getPath() const
        {
            return {};
        }

        void setSettings(const iscore::DeviceSettings& settings)
        {
        }
};

class MockDeviceFactory : public ProtocolFactory
{
    public:
        QString name() const
        {
            return "Mock";
        }

        DeviceInterface* makeDevice(const iscore::DeviceSettings& settings)
        {
            return new MockDevice(settings);
        }

        ProtocolSettingsWidget* makeSettingsWidget()
        {
            return new MockDeviceSettings;
        }

        void serializeProtocolSpecificSettings(
                const QVariant& data,
                const VisitorVariant& visitor) const
        {
        }

        QVariant makeProtocolSpecificSettings(
                const VisitorVariant& visitor) const
        {
            return {};
        }

        bool checkCompatibility(
                const iscore::DeviceSettings& a,
                const iscore::DeviceSettings& b) const
        {
            return true;
        }
};

