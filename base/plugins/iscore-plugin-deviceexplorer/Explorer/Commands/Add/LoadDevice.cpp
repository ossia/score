#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <algorithm>

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include "LoadDevice.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/TreeNode.hpp>

LoadDevice::LoadDevice(
        Path<DeviceDocumentPlugin>&& device_tree,
        Device::Node&& node):
    m_devicesModel{std::move(device_tree)},
    m_deviceNode(std::move(node))
{
}

void LoadDevice::undo() const
{
    auto& devplug = m_devicesModel.find();
    devplug.updateProxy.removeDevice(m_deviceNode.get<Device::DeviceSettings>());
}

void LoadDevice::redo() const
{
    auto& devplug = m_devicesModel.find();
    devplug.updateProxy.loadDevice(m_deviceNode);
}

void LoadDevice::serializeImpl(DataStreamInput& d) const
{
    d << m_devicesModel;
    d << m_deviceNode;
}

void LoadDevice::deserializeImpl(DataStreamOutput& d)
{
    d >> m_devicesModel;
    d >> m_deviceNode;
}
