#include <QByteArray>
#include <algorithm>

#include "Add/LoadDevice.hpp"
#include "Device/Address/AddressSettings.hpp"
#include "Device/Node/DeviceNode.hpp"
#include "Remove.hpp"
#include "Remove/RemoveAddress.hpp"
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>

class DeviceDocumentPlugin;

using namespace DeviceExplorer::Command;

Remove::Remove(
        Path<DeviceDocumentPlugin> device_tree,
        const iscore::Node& node)
{
    ISCORE_ASSERT(!node.is<InvisibleRootNodeTag>());

    if (node.is<iscore::AddressSettings>())
    {
        m_device = false;
        m_cmd = new RemoveAddress{
                    std::move(device_tree),
                    iscore::NodePath{node}};
    }
    else
    {
        m_device = true;
        m_cmd = new LoadDevice{
                    std::move(device_tree),
                    iscore::Node{node}};
    }
}

Remove::~Remove()
{
    delete m_cmd;
}

void Remove::undo() const
{
    m_device ? m_cmd->redo() : m_cmd->undo();
}

void Remove::redo() const
{
    m_device ? m_cmd->undo() : m_cmd->redo();
}

void Remove::serializeImpl(DataStreamInput& d) const
{
    d << m_device
      << m_cmd->serialize();
}

void Remove::deserializeImpl(DataStreamOutput& d)
{
    QByteArray cmd_data;
    d >> m_device
      >> cmd_data;

    if(m_device)
    {
        m_cmd = new LoadDevice;
    }
    else
    {
        m_cmd = new RemoveAddress;
    }

    m_cmd->deserialize(cmd_data);
}
