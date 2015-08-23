//#include "Insert.hpp"
//#include "Add/AddDevice.hpp"
//#include "Add/AddAddress.hpp"

//using namespace DeviceExplorer::Command;
//using namespace iscore;
//Insert::Insert(
//        const NodePath &parentPath,
//        int row,
//        Node &&data,
//        ModelPath<DeviceDocumentPlugin>&& modelPath):
//    iscore::SerializableCommand{factoryName(),
//                                commandName(),
//                                description()},
//    m_model{std::move(modelPath)},
//    m_node{std::move(data)},
//    m_parentPath{parentPath},
//    m_row{row}
//{

//}

//void
//Insert::undo()
//{
//    auto& model = m_model.find();

//    QModelIndex parentIndex = model.convertPathToIndex(m_parentPath);

//    const bool result = model.removeRows(m_row, 1, parentIndex);

//    model.setCachedResult(result);

//}

//void recurse_addAddress(const ObjectPath& model, const Node& n, NodePath nodePath)
//{

//    AddAddress addr{
//                model,
//                nodePath,
//                DeviceExplorerModel::Insert::AsChild,
//                n.get<AddressSettings>()};
//    addr.redo();

//    nodePath.append(addr.createdNodeIndex());
//    for(const auto& child : n.children())
//    {
//        recurse_addAddress(model, *child, nodePath);
//    }

//}

//template<typename T>
//auto copy(const T&t)
//{
//    return T(t);
//}

//void
//Insert::redo()
//{
//    if(m_node.is<DeviceSettings>())
//    {
//        AddDevice dev{copy(m_model), m_node.get<DeviceSettings>()};
//        dev.redo();

//        NodePath p;
//        p.append(dev.deviceRow());
//        for(auto& child : m_node.children())
//        {
//            recurse_addAddress(m_model, *child, p);
//        }
//    }
//    else
//    {
//        recurse_addAddress(ObjectPath{m_model}, m_node, m_parentPath);
//    }
//}

//void
//Insert::serializeImpl(QDataStream& d) const
//{
//    d << m_model << m_node << m_parentPath << m_row;
//}

//void
//Insert::deserializeImpl(QDataStream& d)
//{
//    d >> m_model >> m_node >> m_parentPath >> m_row;
//}
