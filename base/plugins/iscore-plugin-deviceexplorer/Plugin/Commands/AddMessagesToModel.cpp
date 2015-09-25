#include "AddMessagesToModel.hpp"
#include <DeviceExplorer/ItemModels/MessageItemModel.hpp>

AddMessagesToModel::AddMessagesToModel(
        Path<iscore::MessageItemModel>&& path,
        const iscore::MessageList& messages) :
    SerializableCommand {factoryName(),
                         commandName(),
                         description()},
    m_path{std::move(path)}
{
    // Here the state might not exist yet,
    // if drag'n'dropping in a scenario for instance.
    // We can them assume that the created state would have been an empty state.
    auto model = m_path.try_find();
    if(model)
    {
        m_old = model->rootNode();
    }
    m_new = merge(m_old, messages);
}


void AddMessagesToModel::undo()
{
    auto& model = m_path.find();
    model = m_old;
}

void AddMessagesToModel::redo()
{
    auto& model = m_path.find();
    model = m_new;
}

void AddMessagesToModel::serializeImpl(QDataStream& s) const
{
    s << m_path << m_old << m_new;
}

void AddMessagesToModel::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_old >> m_new;
}
