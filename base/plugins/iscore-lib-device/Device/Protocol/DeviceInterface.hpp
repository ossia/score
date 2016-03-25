#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <State/Address.hpp>
#include <boost/optional/optional.hpp>
#include <QObject>
#include <QString>
#include <vector>

#include <State/Value.hpp>
#include <iscore_lib_device_export.h>

namespace State
{
struct Message;
}
namespace Device
{
struct FullAddressSettings;


struct ISCORE_LIB_DEVICE_EXPORT DeviceCapas
{
        bool canAddNode{true};
        bool canRemoveNode{true};
        bool canRenameNode{true};
        bool canDisconnect{true};
        bool canRefreshValue{true};
        bool canRefreshTree{false};
        bool canListen{true};
};

class ISCORE_LIB_DEVICE_EXPORT DeviceInterface : public QObject
{
        Q_OBJECT

    public:
        explicit DeviceInterface(const Device::DeviceSettings& s);
        virtual ~DeviceInterface();

        const Device::DeviceSettings& settings() const;

        virtual void addNode(const Device::Node& n);

        auto capabilities() const { return m_capas; }

        virtual void disconnect() = 0;
        virtual bool reconnect() = 0;
        virtual bool connected() const = 0;

        virtual void updateSettings(const Device::DeviceSettings&) = 0;

        // Asks, and returns all the new addresses if the device can refresh itself Minuit-like.
        // The addresses are not applied to the device, they have to be via a command!
        virtual Device::Node refresh() { return {}; }
        virtual boost::optional<State::Value> refresh(const State::Address&) { return {}; }
        virtual void setListening(const State::Address&, bool) { }
        virtual void addToListening(const std::vector<State::Address>&) { }
        virtual std::vector<State::Address> listening() const { return {}; }

        virtual void addAddress(const Device::FullAddressSettings&) = 0;
        virtual void updateAddress(
                const State::Address& currentAddr,
                const Device::FullAddressSettings& newAddr) = 0;
        virtual void removeNode(const State::Address&) = 0;

        // Execution API... Maybe we don't need it here.
        virtual void sendMessage(const State::Message& mess) = 0;
        virtual bool check(const QString& str) = 0;

        // Make a node from an inside path, if it has been added for instance.
        virtual Device::Node getNode(const State::Address&) = 0;

    signals:
        // These signals are emitted if a device changes from the inside
        void pathAdded(const State::Address&);
        void pathUpdated(
                const State::Address&, // current address
                const Device::AddressSettings&); // new data
        void pathRemoved(const State::Address&);

        void valueUpdated(const State::Address&, const State::Value&);

        // In case the whole namespace changed?
        void namespaceUpdated();

    protected:
        Device::DeviceSettings m_settings;
        DeviceCapas m_capas;
};
}
