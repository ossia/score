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

namespace iscore {
struct FullAddressSettings;
struct Message;
}  // namespace iscore

class ISCORE_LIB_DEVICE_EXPORT DeviceInterface : public QObject
{
        Q_OBJECT

    public:
        explicit DeviceInterface(const iscore::DeviceSettings& s);
        virtual ~DeviceInterface();

        const iscore::DeviceSettings& settings() const;

        virtual void addNode(const iscore::Node& n);

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

        // Make a node from an inside path, if it has been added for instance.
        virtual iscore::Node getNode(const iscore::Address&) = 0;

    signals:
        // These signals are emitted if a device changes from the inside
        void pathAdded(const iscore::Address&);
        void pathUpdated(const iscore::FullAddressSettings&);
        void pathRemoved(const iscore::Address&);

        void valueUpdated(const iscore::Address&, const iscore::Value&);

        // In case the whole namespace changed?
        void namespaceUpdated();

    protected:
        iscore::DeviceSettings m_settings;
};
