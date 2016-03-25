#include "ListeningManager.hpp"
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Explorer/DeviceExplorerView.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Device/Protocol/DeviceInterface.hpp>

namespace Explorer
{
Device::DeviceInterface& ListeningManager::deviceFromProxyModelIndex(
        const QModelIndex& idx)
{
    return deviceFromNode(nodeFromProxyModelIndex(idx));
}

Device::DeviceInterface& ListeningManager::deviceFromModelIndex(
        const QModelIndex& idx)
{
    return deviceFromNode(m_model.nodeFromModelIndex(idx));
}

Device::Node& ListeningManager::nodeFromProxyModelIndex(
        const QModelIndex& idx)
{
    return m_model.nodeFromModelIndex(m_widget.sourceIndex(idx));
}

Device::Node& ListeningManager::nodeFromModelIndex(
        const QModelIndex& idx)
{
    return m_model.nodeFromModelIndex(idx);
}


ListeningManager::ListeningManager(
        DeviceExplorerModel& model,
        const DeviceExplorerWidget& widg):
    m_model{model},
    m_widget{widg},
    m_handler{m_model.deviceModel().listening()}
{
    connect(&m_handler, &ListeningHandler::stop,
            this, &ListeningManager::stopListening);
    connect(&m_handler, &ListeningHandler::restore,
            this, &ListeningManager::setDeviceWidgetListening);
}

void ListeningManager::enableListening(
        Device::Node& node)
{
    auto& dev = deviceFromNode(node);

    auto addr = Device::address(node);
    m_handler.setListening(dev, addr, true);
}

void ListeningManager::disableListening_rec(
        const Device::Node& node,
        Device::DeviceInterface& dev,
        ListeningHandler& lm)
{
    if(node.is<Device::AddressSettings>())
    {
        auto addr = Device::address(node);

        lm.setListening(dev, addr, false);
    }

    for(const auto& child : node)
    {
        disableListening_rec(child, dev, lm);
    }
}

void ListeningManager::enableListening_rec(
        const QModelIndex& index,
        Device::DeviceInterface& dev,
        ListeningHandler& lm)
{
    int i = 0;
    for(const auto& child : nodeFromProxyModelIndex(index))
    {
        if(child.is<Device::AddressSettings>())
        {
            auto addr = Device::address(child);
            lm.setListening(dev, addr, true);
        }

        // TODO check this
        auto childIndex = index.child(i, 0);

        if(m_widget.view()->isExpanded(childIndex))
        {
            enableListening_rec(childIndex, dev, lm);
        }
        i++;
    }
}

Device::DeviceInterface&ListeningManager::deviceFromNode(
        const Device::Node& node)
{
    auto& list = m_model.deviceModel().list();
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


void ListeningManager::setListening(
        const QModelIndex& idx, bool b)
{
    auto& dev = deviceFromProxyModelIndex(idx);
    if(b)
    {
        enableListening_rec(idx, dev, m_handler);
    }
    else
    {
        for(const auto& child : nodeFromProxyModelIndex(idx))
        {
            disableListening_rec(child, dev, m_handler);
        }
    }
}

void ListeningManager::resetListening(
        Device::Node& node)
{
    auto idx = m_model.modelIndexFromNode(node, 0);
    auto& dev = deviceFromModelIndex(idx);

    for(const auto& child : node)
    {
        disableListening_rec(child, dev, m_handler);
    }

    auto view_idx = m_widget.proxyIndex(idx);
    if(m_widget.view()->isExpanded(view_idx))
    {
        enableListening_rec(view_idx, dev, m_handler);
    }
}

void ListeningManager::stopListening()
{
    for(auto device : m_model.deviceModel().list().devices())
    {
        auto vec = device->listening();
        for(const auto& elt : vec)
            device->setListening(elt, false);
    }
}

void ListeningManager::setDeviceWidgetListening()
{
    for(auto& device : m_model.rootNode())
    {
        resetListening(device);
    }
}
}
