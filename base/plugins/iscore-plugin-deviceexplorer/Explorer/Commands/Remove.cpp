#include <QByteArray>
#include <algorithm>

#include "Add/LoadDevice.hpp"
#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include "Remove.hpp"
#include "Remove/RemoveAddress.hpp"
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>


namespace DeviceExplorer
{
class DeviceDocumentPlugin;
namespace Command
{
Remove::Remove(Path<DeviceDocumentPlugin> device_tree, Device::NodePath&& path):
    m_device{false},
    m_cmd{new RemoveAddress{
          std::move(device_tree),
          std::move(path)}}
{

}

Remove::Remove(Path<DeviceDocumentPlugin> device_tree, const Device::Node& node):
    m_device{true},
    m_cmd{new LoadDevice{
          std::move(device_tree),
          Device::Node{node}}}
{

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
}
}
