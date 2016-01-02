#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <QString>

#include <State/Value.hpp>
#include <iscore_plugin_deviceexplorer_export.h>

class DeviceDocumentPlugin;
class DeviceExplorerModel;
namespace State {
struct Address;
}

namespace Device
{
struct AddressSettings;
struct DeviceSettings;
}
/**
 * @brief The NodeUpdateProxy class
 *
 * This class is used to correctly add and
 * remove elements to the node hierarchy.
 * If there is no device explorer model yet (e.g. while restoring from a crash),
 * we can simply add them where required
 * to the root node and they will be loaded afterwards.
 * But when there is a device explorer model, we have
 * to use its methods (that calls beginAddRows, etc.) to prevent a full refresh
 * which would be laggy.
 */
class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT NodeUpdateProxy
{
    public:
        DeviceDocumentPlugin& devModel;
        DeviceExplorerModel* deviceExplorer{};

        explicit NodeUpdateProxy(DeviceDocumentPlugin& root);
        NodeUpdateProxy(const NodeUpdateProxy&) = delete;
        NodeUpdateProxy(NodeUpdateProxy&&) = delete;
        NodeUpdateProxy& operator=(const NodeUpdateProxy&) = delete;
        NodeUpdateProxy& operator=(NodeUpdateProxy&&) = delete;

        void addDevice(const Device::DeviceSettings& dev);
        void loadDevice(const Device::Node& node);

        void updateDevice(
                const QString& name,
                const Device::DeviceSettings& dev);

        void removeDevice(const Device::DeviceSettings& dev);

        /**
         * @brief addAddress Adds a single address in the tree
         * @param parentPath Path to the parent
         * @param settings Informations of the address
         *
         * Used to add a new address (after input from user for instance)
         */
        void addAddress(
                const Device::NodePath& parentPath,
                const Device::AddressSettings& settings,
                int row);

        /**
         * @brief addNode Adds a node hierarchy in the tree
         * @param parentPath Path to the parent
         * @param node The node to insert.
         */
        void addNode(
                const Device::NodePath& parentPath,
                const Device::Node& node,
                int row);

        void updateAddress(
                const Device::NodePath& nodePath,
                const Device::AddressSettings& settings);

        void removeNode(
                const Device::NodePath& parentPath,
                const Device::AddressSettings& settings);


        // Local : the Device::Node structure
        // Remote : what's behind a DeviceInterface
        void addLocalAddress(
                Device::Node& parent,
                const Device::AddressSettings& data,
                int row);

        void addLocalNode(
                Device::Node& parent,
                Device::Node&& node);

        void removeLocalNode(
                const State::Address&);
        void updateLocalValue(
                const State::Address&,
                const State::Value&);
        void updateLocalSettings(
                const State::Address&,
                const Device::AddressSettings&);

        void updateRemoteValue(
                const State::Address&,
                const State::Value&);

        State::Value refreshRemoteValue(
                const State::Address&);
        void refreshRemoteValues(
                const Device::NodeList&);


    private:
        void rec_addNode(
                Device::NodePath parentPath,
                const Device::Node& node,
                int row);
};
