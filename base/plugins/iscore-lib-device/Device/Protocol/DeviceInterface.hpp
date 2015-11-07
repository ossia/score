#pragma once
#include <QString>
#include <State/Message.hpp>
#include <State/Address.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <memory>

#include <Device/Node/DeviceNode.hpp>
class DeviceInterface : public QObject
{
        Q_OBJECT

    public:
        explicit DeviceInterface(const iscore::DeviceSettings& s);
        virtual ~DeviceInterface();

        const iscore::DeviceSettings& settings() const;

        virtual void addNode(const iscore::Node& n)
        {
            auto full = iscore::FullAddressSettings::make<iscore::FullAddressSettings::as_parent>(
                            n.get<iscore::AddressSettings>(), iscore::address(*n.parent()));

            // Add in the device implementation
            addAddress(full);

            for(const auto& child : n)
            {
                addNode(child);
            }
        }

        virtual void disconnect() = 0;
        virtual bool reconnect() = 0;
        virtual bool connected() const = 0;

        virtual void updateSettings(const iscore::DeviceSettings&) = 0;

        // Asks, and returns all the new addresses if the device can refresh itself Minuit-like.
        // The addresses are not applied to the device, they have to be via a command!
        virtual bool canRefresh() const { return false; }
        virtual iscore::Node refresh() { return {}; }
        virtual boost::optional<iscore::Value> refresh(const iscore::Address&) { return {}; }
        virtual void setListening(const iscore::Address&, bool) { }
        virtual void replaceListening(const std::vector<iscore::Address>&) { }
        virtual std::vector<iscore::Address> listening() const { return {}; }

        virtual void addAddress(const iscore::FullAddressSettings&) = 0;
        virtual void updateAddress(const iscore::FullAddressSettings&) = 0;
        virtual void removeNode(const iscore::Address&) = 0;

        // Execution API... Maybe we don't need it here.
        virtual void sendMessage(const iscore::Message& mess) = 0;
        virtual bool check(const QString& str) = 0;

    signals:
        // These signals are emitted if a device changes from the inside
        void pathAdded(const iscore::FullAddressSettings&);
        void pathUpdated(const iscore::FullAddressSettings&);
        void pathRemoved(const QString&);

        void valueUpdated(const iscore::Address&, const iscore::Value&);

        // In case the whole namespace changed?
        void namespaceUpdated();

    protected:
        iscore::DeviceSettings m_settings;
};
