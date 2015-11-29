#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <algorithm>

#include "Device/Address/AddressSettings.hpp"
#include "Device/Node/DeviceNode.hpp"
#include "Explorer/DocumentPlugin/NodeUpdateProxy.hpp"
#include "RemoveAddress.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/TreeNode.hpp>
#include <iscore/tools/TreePath.hpp>

RemoveAddress::RemoveAddress(
        Path<DeviceDocumentPlugin>&& device_tree,
        const iscore::NodePath& nodePath):
    m_devicesModel{device_tree},
    m_nodePath{nodePath}
{
    auto& devplug = m_devicesModel.find();

    auto n = nodePath.toNode(&devplug.rootNode());
    ISCORE_ASSERT(n);
    ISCORE_ASSERT(n->is<iscore::AddressSettings>());
    m_savedNode = *n;
}

void RemoveAddress::undo() const
{
    auto& devplug = m_devicesModel.find();
    auto parentPath = m_nodePath;
    parentPath.removeLast();

    devplug.updateProxy.addNode(parentPath, m_savedNode, m_nodePath.back());
}

void RemoveAddress::redo() const
{
    auto& devplug = m_devicesModel.find();
    auto parentPath = m_nodePath;
    parentPath.removeLast();
    devplug.updateProxy.removeNode(
                parentPath,
                m_savedNode.get<iscore::AddressSettings>());
}

void RemoveAddress::serializeImpl(DataStreamInput &s) const
{
    s << m_devicesModel
      << m_nodePath
      << m_savedNode;
}

void RemoveAddress::deserializeImpl(DataStreamOutput &s)
{
    s >> m_devicesModel
      >> m_nodePath
      >> m_savedNode;
}
