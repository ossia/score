#pragma once
#include <QString>
#include <DeviceExplorer/Node/Node.hpp>
// This class is used to correctly add and
// remove elements to the node hierarchy.
// If there is no device explorer model yet (e.g. while restoring from a crash),
// we can simply add them where required
// to the root node and they will be loaded afterwards.
// But when there is a device explorer model, we have
// to use its methods (that calls beginAddRows, etc.) to prevent a full refresh
// which would be laggy
class DeviceExplorerModel;
class DeviceDocumentPlugin;

namespace iscore
{
class DeviceSettings;
class AddressSettings;
}
class NodeUpdateProxy
{
    public:
        DeviceDocumentPlugin& m_devModel;
        DeviceExplorerModel* m_deviceExplorer{};

        NodeUpdateProxy(DeviceDocumentPlugin& root);

        void addDevice(const iscore::DeviceSettings& dev);
        void loadDevice(const iscore::Node& node);

        void updateDevice(
                const QString& name,
                const iscore::DeviceSettings& dev);

        void removeDevice(const iscore::DeviceSettings& dev);

        void addAddress(
                const iscore::NodePath& parentPath,
                const iscore::AddressSettings& settings);

        void updateAddress(
                const iscore::NodePath& nodePath,
                const iscore::AddressSettings& settings);

        void removeAddress(
                const iscore::NodePath& parentPath,
                const iscore::AddressSettings& settings);
};
