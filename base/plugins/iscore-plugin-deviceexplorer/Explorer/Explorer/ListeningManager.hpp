#pragma once
#include <Device/Node/DeviceNode.hpp>
namespace Device
{
class DeviceInterface;
}
namespace DeviceExplorer
{
class DeviceExplorerWidget;
class ListeningManager
{
        const DeviceExplorerWidget& m_widg;

    public:
        ListeningManager(
                const DeviceExplorerWidget& widg);

        void enableListening(Device::Node& node);
        void setListening(const QModelIndex& idx, bool b);
        void resetListening(Device::Node& idx);

    private:
        void disableListening_rec(const Device::Node& node, Device::DeviceInterface&);
        void enableListening_rec(const QModelIndex& index, Device::DeviceInterface&);

        Device::DeviceInterface& deviceFromNode(const Device::Node&);
        Device::DeviceInterface& deviceFromProxyModelIndex(const QModelIndex&);
        Device::DeviceInterface& deviceFromModelIndex(const QModelIndex& idx);

        Device::Node& nodeFromProxyModelIndex(const QModelIndex&);
        Device::Node& nodeFromModelIndex(const QModelIndex&);

};
}
