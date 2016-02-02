#include "ListeningManager.hpp"
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Explorer/DeviceExplorerView.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>


namespace Explorer
{

ListeningManager::ListeningManager(
        const DeviceExplorerWidget& widg):
    m_widg{widg}
{

}

void ListeningManager::enableListening(
        Device::Node& node)
{
    auto& dev = deviceFromNode(node);

    auto addr = Device::address(node);
    dev.setListening(addr, true);
}

void ListeningManager::disableListening_rec(
        const Device::Node& node,
        Device::DeviceInterface& dev)
{
    if(node.is<Device::AddressSettings>())
    {
        auto addr = Device::address(node);
        dev.setListening(addr, false);
    }

    for(const auto& child : node)
    {
        disableListening_rec(child, dev);
    }
}

void ListeningManager::enableListening_rec(
        const QModelIndex& index,
        Device::DeviceInterface& dev)
{
    int i = 0;
    for(const auto& child : nodeFromProxyModelIndex(index))
    {
        if(child.is<Device::AddressSettings>())
        {
            auto addr = Device::address(child);
            dev.setListening(addr, true);
        }

        // TODO check this
        auto childIndex = index.child(i, 0);

        if(m_widg.view()->isExpanded(childIndex))
        {
            enableListening_rec(childIndex, dev);
        }
        i++;
    }
}

Device::DeviceInterface&ListeningManager::deviceFromNode(
        const Device::Node& node)
{
    auto& list = m_widg.model()->deviceModel().list();
    if(node.is<Device::AddressSettings>())
    {
        // OPTIMIZEME by just going to the top node
        auto addr = Device::address(node);
        return list.device(addr.device);
    }
    else if(node.is<Device::DeviceSettings>())
    {
        return list.device(node.get<Device::DeviceSettings>().name);
    }

    ISCORE_ABORT;
}

Device::DeviceInterface& ListeningManager::deviceFromProxyModelIndex(
        const QModelIndex& idx)
{
    return deviceFromNode(nodeFromProxyModelIndex(idx));
}

Device::DeviceInterface& ListeningManager::deviceFromModelIndex(
        const QModelIndex& idx)
{
    return deviceFromNode(m_widg.model()->nodeFromModelIndex(idx));
}

Device::Node& ListeningManager::nodeFromProxyModelIndex(
        const QModelIndex& idx)
{
    return m_widg.model()->nodeFromModelIndex(m_widg.sourceIndex(idx));
}

Device::Node& ListeningManager::nodeFromModelIndex(
        const QModelIndex& idx)
{
    return m_widg.model()->nodeFromModelIndex(idx);
}

void ListeningManager::setListening(
        const QModelIndex& idx, bool b)
{
    auto& dev = deviceFromProxyModelIndex(idx);
    if(b)
    {
        enableListening_rec(idx, dev);
    }
    else
    {
        for(const auto& child : nodeFromProxyModelIndex(idx))
        {
            disableListening_rec(child, dev);
        }
    }
}

void ListeningManager::resetListening(
        Device::Node& node)
{
    auto idx = m_widg.model()->modelIndexFromNode(node, 0);
    auto& dev = deviceFromModelIndex(idx);

    for(const auto& child : node)
    {
        disableListening_rec(child, dev);
    }

    if(m_widg.view()->isExpanded(idx))
    {
        enableListening_rec(idx, dev);
    }
}
}
