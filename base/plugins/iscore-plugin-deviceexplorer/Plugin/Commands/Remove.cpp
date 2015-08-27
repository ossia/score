#include "Remove.hpp"

using namespace DeviceExplorer::Command;

Remove::Remove(
        Path<DeviceDocumentPlugin>&& device_tree,
        const iscore::Node& node):
    iscore::SerializableCommand{factoryName(),
                            commandName(),
                            description()}
{
    if (!node.is<iscore::DeviceSettings>())
    {
        m_device = false;
        m_cmd = new AddAddress{
                    std::move(device_tree),
                    iscore::NodePath{*node.parent()},
                    InsertMode::AsChild,
                    node.get<iscore::AddressSettings>()};
    }
    else
    {
        m_device = true;
        m_cmd = new AddDevice{
                    std::move(device_tree),
                    node.get<iscore::DeviceSettings>()};
    }
}

Remove::~Remove()
{
    delete m_cmd;
}

void Remove::undo()
{
    m_cmd->redo();

}

void Remove::redo()
{
    m_cmd->undo();
}

void Remove::serializeImpl(QDataStream& d) const
{
    d << m_device
      << m_cmd->serialize();
}

void Remove::deserializeImpl(QDataStream& d)
{
    QByteArray cmd_data;
    d >> m_device
      >> cmd_data;

    if(m_device)
    {
        m_cmd = new AddDevice;
    }
    else
    {
        m_cmd = new AddAddress;
    }

    m_cmd->deserialize(cmd_data);
}
