
#include "Insert.hpp"
#include "Add/AddDevice.hpp"
#include "Add/AddAddress.hpp"
using namespace DeviceExplorer::Command;
using namespace iscore;
Insert::Insert(const Path &parentPath,
               int row,
               Node &&data,
               ObjectPath&& modelPath):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                this->commandName(),
                                this->description()},
    m_model{std::move(modelPath)},
    m_node{std::move(data)},
    m_parentPath{parentPath},
    m_row{row}
{

}


void
Insert::undo()
{
    auto& model = m_model.find<DeviceExplorerModel>();

    QModelIndex parentIndex = model.convertPathToIndex(m_parentPath);

    const bool result = model.removeRows(m_row, 1, parentIndex);

    model.setCachedResult(result);
}

void recurse_addAddress(const ObjectPath& model, const Node& n, Path nodePath)
{
    AddAddress addr{
        ObjectPath(model),
                nodePath,
                DeviceExplorerModel::Insert::AsChild,
                n.addressSettings()};
    addr.redo();

    nodePath.append(addr.createdNodeIndex());
    for(const auto& child : n.children())
    {
        recurse_addAddress(model, *child, nodePath);
    }
}

void
Insert::redo()
{
    if(m_node.isDevice())
    {
        AddDevice dev{ObjectPath{m_model}, m_node.deviceSettings()};
        dev.redo();

        Path p;
        p.append(dev.deviceRow());
        for(auto& child : m_node.children())
        {
            recurse_addAddress(m_model, *child, p);
        }
    }
    else
    {
        recurse_addAddress(ObjectPath{m_model}, m_node, m_parentPath);
    }
}

void
Insert::serializeImpl(QDataStream& d) const
{
    d << m_model << m_node << m_parentPath << m_row;
    /*
    // TODO keep this "read/write raw data" in mind for other places!!
    d << (qint32) m_data.size();
    d.writeRawData(m_data.data(), m_data.size());
    */
}

void
Insert::deserializeImpl(QDataStream& d)
{
    d >> m_model >> m_node >> m_parentPath >> m_row;
    /*
    d >> v;
    int size = v;
    m_data.resize(size);
    d.readRawData(m_data.data(), size);
    */
}
