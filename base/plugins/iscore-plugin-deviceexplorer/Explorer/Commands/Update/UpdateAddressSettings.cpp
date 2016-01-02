#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include "UpdateAddressSettings.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/TreeNode.hpp>

using namespace iscore;
using namespace DeviceExplorer::Command;
UpdateAddressSettings::UpdateAddressSettings(
        Path<DeviceDocumentPlugin>&& device_tree,
        const Device::NodePath& node,
        const Device::AddressSettings& parameters):
    m_devicesModel{device_tree},
    m_node(node),
    m_newParameters(parameters)
{
    auto& devplug = m_devicesModel.find();
    auto n = m_node.toNode(&devplug.rootNode());
    ISCORE_ASSERT(n);

    m_oldParameters = n->get<Device::AddressSettings>();
}

void UpdateAddressSettings::undo() const
{
    auto& devplug = m_devicesModel.find();
    devplug.updateProxy.updateAddress(m_node, m_oldParameters);
}

void UpdateAddressSettings::redo() const
{
    auto& devplug = m_devicesModel.find();
    devplug.updateProxy.updateAddress(m_node, m_newParameters);
}

void UpdateAddressSettings::serializeImpl(DataStreamInput& d) const
{
    d << m_devicesModel
      << m_node
      << m_oldParameters
      << m_newParameters;
}

void UpdateAddressSettings::deserializeImpl(DataStreamOutput& d)
{
    d >> m_devicesModel
      >> m_node
      >> m_oldParameters
      >> m_newParameters;
}
