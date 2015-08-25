#include "UpdateAddressSettings.hpp"

#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <boost/range/algorithm/find_if.hpp>

using namespace iscore;
using namespace DeviceExplorer::Command;
UpdateAddressSettings::UpdateAddressSettings(
        ModelPath<DeviceDocumentPlugin>&& device_tree,
        const iscore::NodePath& node,
        const iscore::AddressSettings& parameters):
    iscore::SerializableCommand(this),
    m_devicesModel{device_tree},
    m_node(node),
    m_newParameters(parameters)
{
    auto& devplug = m_devicesModel.find();
    auto n = m_node.toNode(&devplug.rootNode());
    ISCORE_ASSERT(n);

    m_oldParameters = n->get<iscore::AddressSettings>();
}

void UpdateAddressSettings::undo()
{
    auto& devplug = m_devicesModel.find();
    devplug.updateProxy.updateAddress(m_node, m_oldParameters);
}

void UpdateAddressSettings::redo()
{
    auto& devplug = m_devicesModel.find();
    devplug.updateProxy.updateAddress(m_node, m_newParameters);
}

void UpdateAddressSettings::serializeImpl(QDataStream& d) const
{
    d << m_devicesModel
      << m_oldParameters
      << m_newParameters;
}

void UpdateAddressSettings::deserializeImpl(QDataStream& d)
{
    d >> m_devicesModel
      >> m_oldParameters
      >> m_newParameters;
}
