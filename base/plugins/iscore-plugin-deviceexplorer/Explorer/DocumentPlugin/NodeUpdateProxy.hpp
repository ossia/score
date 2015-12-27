#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <QString>

#include <State/Value.hpp>
#include <iscore_plugin_deviceexplorer_export.h>

class DeviceDocumentPlugin;
class DeviceExplorerModel;
namespace iscore {
struct Address;
}  // namespace iscore

namespace iscore
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

        void addDevice(const iscore::DeviceSettings& dev);
        void loadDevice(const iscore::Node& node);

        void updateDevice(
                const QString& name,
                const iscore::DeviceSettings& dev);

        void removeDevice(const iscore::DeviceSettings& dev);

        /**
         * @brief addAddress Adds a single address in the tree
         * @param parentPath Path to the parent
         * @param settings Informations of the address
         *
         * Used to add a new address (after input from user for instance)
         */
        void addAddress(
                const iscore::NodePath& parentPath,
                const iscore::AddressSettings& settings,
                int row);

        /**
         * @brief addNode Adds a node hierarchy in the tree
         * @param parentPath Path to the parent
         * @param node The node to insert.
         */
        void addNode(
                const iscore::NodePath& parentPath,
                const iscore::Node& node,
                int row);

        void requestUpdateAddress(
                const iscore::NodePath& nodePath,
                const iscore::AddressSettings& settings);

        void removeNode(
                const iscore::NodePath& parentPath,
                const iscore::AddressSettings& settings);


        // Local : the iscore::Node structure
        // Remote : what's behind a DeviceInterface
        void addLocalAddress(
                iscore::Node& parent,
                const iscore::AddressSettings& data,
                int row);

        void addLocalNode(
                iscore::Node& parent,
                iscore::Node&& node);

        void removeLocalNode(
                const iscore::Address&);
        void updateLocalValue(
                const iscore::Address&,
                const iscore::Value&);

        void updateRemoteValue(
                const iscore::Address&,
                const iscore::Value&);

        iscore::Value refreshRemoteValue(
                const iscore::Address&);
        void refreshRemoteValues(
                const iscore::NodeList&);


    private:
        void rec_addNode(
                iscore::NodePath parentPath,
                const iscore::Node& node,
                int row);
};
