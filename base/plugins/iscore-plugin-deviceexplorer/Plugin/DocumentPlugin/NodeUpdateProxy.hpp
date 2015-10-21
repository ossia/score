#pragma once
#include <QString>
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>
class DeviceExplorerModel;
class DeviceDocumentPlugin;

namespace iscore
{
struct DeviceSettings;
struct AddressSettings;
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
class NodeUpdateProxy
{
    public:
        DeviceDocumentPlugin& devModel;
        DeviceExplorerModel* deviceExplorer{};

        explicit NodeUpdateProxy(DeviceDocumentPlugin& root);

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

        void updateAddress(
                const iscore::NodePath& nodePath,
                const iscore::AddressSettings& settings);

        void removeNode(
                const iscore::NodePath& parentPath,
                const iscore::AddressSettings& settings);

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
