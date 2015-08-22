#include "LoadDevice.hpp"
#include <Plugin/DeviceExplorerPlugin.hpp>
#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>

LoadDevice::LoadDevice(
        ModelPath<DeviceDocumentPlugin>&& device_tree,
        iscore::Node&& node):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                commandName(),
                                description()},
    m_devicesModel{std::move(device_tree)},
    m_deviceNode{std::move(node)}
{
}

void LoadDevice::undo()
{
    auto& devplug = m_devicesModel.find();
    devplug.updateProxy.removeDevice(m_deviceNode.deviceSettings());
}

void LoadDevice::redo()
{
    auto& devplug = m_devicesModel.find();
    devplug.updateProxy.loadDevice(m_deviceNode);
}

void LoadDevice::serializeImpl(QDataStream& d) const
{
    d << m_devicesModel;
    d << m_deviceNode;
}

void LoadDevice::deserializeImpl(QDataStream& d)
{
    d >> m_devicesModel;
    d >> m_deviceNode;
}
