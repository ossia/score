#include <Device/ItemModels/NodeDisplayMethods.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/Commands/Update/UpdateAddressSettings.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <iscore/command/CommandStackFacade.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QAbstractProxyModel>
#include <QApplication>
#include <QDebug>
#include <QFlags>
#include <QJsonDocument>
#include <QMap>
#include <QMimeData>
#include <QObject>

#include <QSet>
#include <QTreeView>
#include <qtypetraits.h>
#include <QVector>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include <Device/Address/AddressSettings.hpp>
#include <Device/ItemModels/NodeBasedItemModel.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include "DeviceExplorerMimeTypes.hpp"
#include "DeviceExplorerModel.hpp"
#include "DeviceExplorerView.hpp"
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <State/ValueConversion.hpp>
#include "Widgets/DeviceEditDialog.hpp" // TODO why here??!!
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/MimeVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/TreeNode.hpp>
#include <State/MessageListSerialization.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>



namespace Explorer
{
static const QMap<Explorer::Column, QString> HEADERS{
    {Explorer::Column::Name, QObject::tr("Address")},
    {Explorer::Column::Value, QObject::tr("Value")},
    {Explorer::Column::Get, QObject::tr("Get")},
    {Explorer::Column::Set, QObject::tr("Set")},
    {Explorer::Column::Min, QObject::tr("Min")},
    {Explorer::Column::Max, QObject::tr("Max")}
};

DeviceExplorerModel::DeviceExplorerModel(
        DeviceDocumentPlugin& plug,
        QObject* parent)
    : NodeBasedItemModel{parent},
      m_lastCutNodeIsCopied{false},
      m_devicePlugin{plug},
      m_rootNode{plug.rootNode()},
      m_cmdQ{&plug.context().commandStack}
{
    this->setObjectName("DeviceExplorerModel");

    beginResetModel();
    endResetModel();
}

DeviceExplorerModel::~DeviceExplorerModel()
{
    for(QStack<CutElt>::iterator it = m_cutNodes.begin();
        it != m_cutNodes.end(); ++it)
    {
        delete it->first;
    }
}

DeviceDocumentPlugin& DeviceExplorerModel::deviceModel() const
{
    return m_devicePlugin;
}

QModelIndexList DeviceExplorerModel::selectedIndexes() const
{
    if(!m_view)
    {
        return {};
    }
    else
    {
        // We have to do this check if we have a proxy
        if (m_view->hasProxy())
        {
            auto indexes = m_view->selectedIndexes();
            for(auto& index : indexes)
                index = static_cast<const QAbstractProxyModel *>(m_view->QTreeView::model())->mapToSource(index);
            return indexes;
        }
        else
        {
             return m_view->selectedIndexes();
        }

    }
}

QStringList
DeviceExplorerModel::getColumns() const
{
    return HEADERS.values();
}

int DeviceExplorerModel::addDevice(const Device::Node& deviceNode)
{
    int row = m_rootNode.childCount();
    QModelIndex parent; //invalid

    beginInsertRows(parent, row, row);
    rootNode().push_back(deviceNode);
    endInsertRows();

    return row;
}

int DeviceExplorerModel::addDevice(Device::Node&& deviceNode)
{
    deviceNode.setParent(&rootNode());

    int row = m_rootNode.childCount();
    QModelIndex parent; //invalid

    beginInsertRows(parent, row, row);
    rootNode().push_back(std::move(deviceNode));
    endInsertRows();

    return row;
}

void DeviceExplorerModel::updateDevice(
        const QString& name,
        const Device::DeviceSettings& dev)
{
    for(int i = 0; i < m_rootNode.childCount(); i++)
    {
        auto n = &m_rootNode.childAt(i);
        if(n->get<Device::DeviceSettings>().name == name)
        {
            n->set(dev);

            QModelIndex index = createIndex(i, 0, n->parent());
            emit dataChanged(index, index);
            return;
        }
    }
}

void DeviceExplorerModel::addAddress(
        Device::Node* parentNode,
        const Device::AddressSettings& addressSettings,
        int row)
{
    ISCORE_ASSERT(parentNode);
    ISCORE_ASSERT(parentNode != &m_rootNode);

    Device::Node* grandparent = parentNode->parent();
    ISCORE_ASSERT(grandparent);
    int rowParent = grandparent->indexOfChild(parentNode);
    QModelIndex parentIndex = createIndex(rowParent, 0, parentNode);

    beginInsertRows(parentIndex, row, row);

    parentNode->emplace(parentNode->begin() + row, addressSettings, parentNode);

    endInsertRows();
}

void DeviceExplorerModel::addNode(
        Device::Node* parentNode,
        Device::Node&& child,
        int row)
{
    ISCORE_ASSERT(parentNode);
    ISCORE_ASSERT(parentNode != &m_rootNode);

    Device::Node* grandparent = parentNode->parent();
    ISCORE_ASSERT(grandparent);
    int rowParent = grandparent->indexOfChild(parentNode);
    QModelIndex parentIndex = createIndex(rowParent, 0, parentNode);

    beginInsertRows(parentIndex, row, row);

    parentNode->emplace(parentNode->begin() + row, std::move(child));

    endInsertRows();
}

void DeviceExplorerModel::updateAddress(
        Device::Node *node,
        const Device::AddressSettings &addressSettings)
{
    ISCORE_ASSERT(node);
    ISCORE_ASSERT(node != &m_rootNode);

    node->set(addressSettings);

    emit dataChanged(
                modelIndexFromNode(*node, 0),
                modelIndexFromNode(*node, (int)Column::Count));
}

void DeviceExplorerModel::updateValue(Device::Node* n, const State::Value& v)
{
    n->get<Device::AddressSettings>().value = v;

    QModelIndex nodeIndex = modelIndexFromNode(*n, 1);

    emit dataChanged(nodeIndex, nodeIndex);
}

bool DeviceExplorerModel::checkDeviceInstantiatable(
        Device::DeviceSettings& n)
{
    // Request from the protocol factory the protocol to see
    // if it is compatible.
    auto& context = m_devicePlugin.context().app.components;
    auto prot = context.factory<Device::DynamicProtocolList>().get(n.protocol);
    if(!prot)
        return false;

    // Look for other childs in the same protocol.
    return std::none_of(rootNode().begin(), rootNode().end(),
                       [&] (const Device::Node& child) {

        ISCORE_ASSERT(child.is<Device::DeviceSettings>());
        const auto& set = child.get<Device::DeviceSettings>();
        return (set.name == n.name)
                || (set.protocol == n.protocol
                    && !prot->checkCompatibility(n, child.get<Device::DeviceSettings>()));

    });
}

bool DeviceExplorerModel::tryDeviceInstantiation(
        Device::DeviceSettings& set,
        DeviceEditDialog& dial)
{
    while(!checkDeviceInstantiatable(set))
    {
        dial.setSettings(set);
        dial.setEditingInvalidState(true);

        bool ret = dial.exec();
        if(!ret)
        {
            dial.setEditingInvalidState(false);
            return false;
        }

        set = dial.getSettings();
    }

    dial.setEditingInvalidState(true);
    return true;
}

bool DeviceExplorerModel::checkAddressInstantiatable(
        Device::Node& parent,
        const Device::AddressSettings& addr)
{
    ISCORE_ASSERT(!parent.is<InvisibleRootNodeTag>());

    if(addr.name.isEmpty())
        return false;

    return std::none_of(parent.begin(),
                        parent.end(),
                        [&] (const Device::Node& n) {
        return n.get<Device::AddressSettings>().name == addr.name;
    });
}

bool DeviceExplorerModel::checkAddressEditable(
        Device::Node& parent,
        const Device::AddressSettings& before,
        const Device::AddressSettings& after)
{
    ISCORE_ASSERT(!parent.is<InvisibleRootNodeTag>());

    if(after.name.isEmpty())
        return false;

    auto it = std::find_if(
                parent.begin(),
                parent.end(),
                [&] (const Device::Node& n) { return n.get<Device::AddressSettings>().name == after.name; });
    if(it != parent.end())
    {
        //  We didn't change name, it's ok
        if(after.name == before.name)
            return true;
        else
            return false;
    }
    else
    {
        // Ok, no conflicts
        return true;
    }
}

int
DeviceExplorerModel::columnCount() const
{
    return (int)Column::Count;
}

int
DeviceExplorerModel::columnCount(const QModelIndex& /*parent*/) const
{
    return (int)Column::Count;
}

QVariant DeviceExplorerModel::getData(Device::NodePath node, Column column, int role)
{
    QModelIndex index = createIndex(convertPathToIndex(node).row(), (int)column, node.toNode(&rootNode())->parent());
    return data(index, role);
}


// must return an invalid QVariant for cases not handled
QVariant
DeviceExplorerModel::data(const QModelIndex& index, int role) const
{
    const int col = index.column();

    if(col < 0 || col >= (int)Column::Count)
    {
        return QVariant();
    }

    const Device::Node& n = nodeFromModelIndex(index);
    switch((Column)col)
    {
        case Column::Name:
        {
            if(n.is<Device::AddressSettings>())
                return Device::nameColumnData(n, role);
            else
            {
                return Device::deviceNameColumnData(
                            n,
                            deviceModel().list().device(n.get<Device::DeviceSettings>().name),
                            role);
            }
        }

        case Column::Value:
            return Device::valueColumnData(n, role);

        case Column::Get:
            return Device::GetColumnData(n, role);

        case Column::Set:
            return Device::SetColumnData(n, role);

        case Column::Min:
            return Device::minColumnData(n, role);

        case Column::Max:
            return Device::maxColumnData(n, role);

        case Column::Count:
        default :
            ISCORE_ABORT;
            return {};
    }

    return {};
}

QVariant
DeviceExplorerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if(section >= 0 && section < (int)Column::Count)
        {
            return HEADERS[(Column)section];
        }
    }

    return {};
}

Qt::ItemFlags
DeviceExplorerModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = Qt::ItemIsEnabled;
    //by default QAbstractItemModel::flags(index); returns Qt::ItemIsEnabled | Qt::ItemIsSelectable

    if(index.isValid())
    {
        const Device::Node& n = nodeFromModelIndex(index);

        if(n.isSelectable())
        {
            f |= Qt::ItemIsSelectable;
        }

        //we allow drag'n drop only from the name column
        if(index.column() == (int)Column::Name)
        {
            f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
        }
        else if(index.column() == (int)Column::Get)
        {
            f |= Qt::ItemIsUserCheckable;
        }
        else if(index.column() == (int)Column::Set)
        {
            f |= Qt::ItemIsUserCheckable;
        }


        if(n.isEditable())
        {
            f |= Qt::ItemIsEditable;
        }

    }
    else
    {
        //to be able to drop even where there is nothing
        f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;

    }

    return f;
}

/*
  return false if no change was made.
  emit dataChanged() & return true if a change is made.

  Note: this is the function that gets called when the user changes the value
  in the tree.
  It then sends a command that calls editData.
*/
bool DeviceExplorerModel::setData(
        const QModelIndex& index,
        const QVariant& value,
        int role)
{
    if(! index.isValid())
        return false;

    auto& n = nodeFromModelIndex(index);

    if(!n.is<Device::AddressSettings>())
        return false;

    auto col = Explorer::Column(index.column());

    if(role == Qt::EditRole || role == Qt::CheckStateRole)
    {
        if(col == Column::Value)
        {
            // In this case we don't make a command, but we directly push the
            // new value.
            auto copy = State::convert::fromQVariant(value);

            // We may have to convert types.
            const auto& orig = n.get<Device::AddressSettings>().value;
            if(copy.val.which() != orig.val.which()
            && !State::convert::convert(orig, copy))
                return false;

            n.get<Device::AddressSettings>().value = copy;

            // Note : if we want to disable remote updating, we have to do it
            // here (e.g. if this becomes a settings)
            m_devicePlugin.updateProxy.updateRemoteValue(Device::address(n), copy);

            return true;
        }
        else
        {
            // Here we make a command because we change the structure of the tree.
            auto settings = n.get<Device::AddressSettings>();
            if(col == Column::Name)
            {
                const QString s = value.toString();
                if(! s.isEmpty())
                {
                    settings.name = s;
                }
            }

            // TODO uncomment to enable io type edition
/*
            if(role == Qt::CheckStateRole)
            {
                if(col == Column::Get)
                {
                    if(value.value<bool>())
                    {
                        switch(settings.ioType)
                        {
                            case IOType::In:
                                break;
                            case IOType::Out:
                                settings.ioType = IOType::InOut;
                                break;
                            case IOType::InOut:
                                break;
                            default:
                                settings.ioType = IOType::In;
                                break;
                        }
                    }
                    else
                    {
                        switch(settings.ioType)
                        {
                            case IOType::In:
                                settings.ioType = IOType::Invalid;
                                break;
                            case IOType::Out:
                                break;
                            case IOType::InOut:
                                settings.ioType = IOType::Out;
                                break;
                            default:
                                break;
                        }
                    }
                }
                else if(col == Column::Set)
                {
                    if(value.value<bool>())
                    {
                        switch(settings.ioType)
                        {
                            case IOType::In:
                                settings.ioType = IOType::InOut;
                                break;
                            case IOType::Out:
                                break;
                            case IOType::InOut:
                                break;
                            default:
                                settings.ioType = IOType::Out;
                                break;
                        }
                    }
                    else
                    {
                        switch(settings.ioType)
                        {
                            case IOType::In:
                                break;
                            case IOType::Out:
                                settings.ioType = IOType::Invalid;
                                break;
                            case IOType::InOut:
                                settings.ioType = IOType::In;
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
            */

            if(settings != n.get<Device::AddressSettings>())
            {
                // We changed
                m_cmdQ->redoAndPush(new Explorer::Command::UpdateAddressSettings{
                                        this->deviceModel(),
                                        Device::NodePath{n},
                                        settings});
                return true;
            }
        }
    }

    return false;
}

bool
DeviceExplorerModel::setHeaderData(int, Qt::Orientation, const QVariant&, int)
{
    return false; //we prevent editing the (column) headers
}

/**
 * @brief DeviceExplorerModel::editData
 *
 * This functions gets called by the command
 * that edit the columns.
 */
void DeviceExplorerModel::editData(
        const Device::NodePath &path,
        Explorer::Column column,
        const State::Value &value,
        int role)
{
    Device::Node* node = path.toNode(&rootNode());
    ISCORE_ASSERT(node->parent());

    QModelIndex index = createIndex(node->parent()->indexOfChild(node), (int)column, node->parent());

    QModelIndex changedTopLeft = index;
    QModelIndex changedBottomRight = index;

    if(node->is<Device::DeviceSettings>())
        return;

    if(role == Qt::EditRole)
    {
        ISCORE_TODO;
        /*
        if(index.column() == (int)Column::Name)
        {
            const QString s = value.toString();

            if(! s.isEmpty())
            {
                node->get<Device::AddressSettings>().name = s;
            }
        }
        else */if(index.column() == (int)Column::Value)
        {
            node->get<Device::AddressSettings>().value = value;
        }
        // TODO min/max/tags editing
    }

    emit dataChanged(changedTopLeft, changedBottomRight);
}


QModelIndex
DeviceExplorerModel::bottomIndex(const QModelIndex& index) const
{
    auto& node = nodeFromModelIndex(index);

    if(! node.hasChildren())
    {
        return index;
    }

    return bottomIndex(
                createIndex(
                    node.childCount() - 1,
                    index.column(),
                    &node.childAt(node.childCount() - 1)));
}

bool
DeviceExplorerModel::isDevice(QModelIndex index) const
{
    if(!index.isValid())
    {
        return false;
    }

    const Device::Node& n = nodeFromModelIndex(index);
    return n.is<Device::DeviceSettings>();
}

bool
DeviceExplorerModel::isEmpty() const
{
    return m_rootNode.childCount() == 0;
}

bool
DeviceExplorerModel::hasCut() const
{
    return (! m_cutNodes.isEmpty());
}

/*
Drag and drop works by deleting the dragged items and creating a new set of dropped items that match those dragged.
I will/may call insertRows(), removeRows(), dropMimeData(), ...
We define two MimeTypes : address and device.
It allows to distinguish whether we are drag'n dropping devices or addresses.
 */


Qt::DropActions
DeviceExplorerModel::supportedDropActions() const
{
    return (Qt::CopyAction);
}

//Default supportedDragActions() implementation returns supportedDropActions().

Qt::DropActions
DeviceExplorerModel::supportedDragActions() const
{
    return (Qt::CopyAction);
}


QStringList
DeviceExplorerModel::mimeTypes() const
{
    return {iscore::mime::device(), iscore::mime::address()};
}

SelectedNodes DeviceExplorerModel::uniqueSelectedNodes(
        const QModelIndexList& indexes) const
{
    SelectedNodes nodes;
    std::transform(indexes.begin(), indexes.end(),
                   std::back_inserter(nodes.parents),
                   [&] (const QModelIndex& idx) {
        return &nodeFromModelIndex(idx);
    });

    nodes.parents.removeAll(&m_rootNode);

    // Filter messages
    for(auto node : nodes.parents)
    {
        if(node->is<Device::AddressSettings>())
        {
            auto& val = node->get<Device::AddressSettings>();
            if(val.ioType == Device::IOType::Out)
            {
                nodes.messages.append(node);
            }
        }
    }

    // Filter parents
    nodes.parents = filterUniqueParents(nodes.parents);

    return nodes;
}
//method called when a drag is initiated
QMimeData*
DeviceExplorerModel::mimeData(const QModelIndexList& indexes) const
{
    QMimeData* mimeData = new QMimeData;

    auto uniqueNodes = uniqueSelectedNodes(indexes);

    // Now we request an update to the device explorer.
    m_devicePlugin.updateProxy.refreshRemoteValues(uniqueNodes.parents);

    // The "MessagesList" part.
    State::MessageList messages;
    for(const auto& node : uniqueNodes.parents)
    {
        Device::parametersList(*node, messages);
    }
    for(const auto& node : uniqueNodes.messages)
    {
        messages += Device::message(*node);
    }

    if(!messages.empty())
    {
        Mime<State::MessageList>::Serializer s{*mimeData};
        s.serialize(messages);
    }

    // The "Nodes" part.
    // TODO The mime data should also transmit the root address for
    // each node in this case. For now it's useless.
    {
        Mime<Device::NodeList>::Serializer s{*mimeData};
        s.serialize(uniqueNodes.parents + uniqueNodes.messages);
    }

    if(messages.empty() && uniqueNodes.parents.empty() && uniqueNodes.messages.empty())
    {
        delete mimeData;
        return nullptr;
    }

    return mimeData;
}


bool
DeviceExplorerModel::canDropMimeData(const QMimeData* mimeData,
                                     Qt::DropAction action,
                                     int /*row*/, int /*column*/,
                                     const QModelIndex& parent) const
{
    if(action == Qt::IgnoreAction)
    {
        return true;
    }

    if(action != Qt::MoveAction && action != Qt::CopyAction)
    {
        return false;
    }

    if(! mimeData || (! mimeData->hasFormat(iscore::mime::device()) && ! mimeData->hasFormat(iscore::mime::address())))
    {
        return false;
    }


    const Device::Node& parentNode = nodeFromModelIndex(parent);

    if(mimeData->hasFormat(iscore::mime::address()))
    {
        if(&parentNode == &m_rootNode)
        {
            return false;
        }
    }
    else
    {
        ISCORE_ASSERT(mimeData->hasFormat(iscore::mime::device()));

        if(&parentNode != &m_rootNode)
        {
            return false;
        }
    }

    return true;
}

//method called when a drop occurs
//return true if drop really handled, false otherwise.
//
// if dropMimeData returns true && action==Qt::MoveAction, removeRows is called immediately after
bool
DeviceExplorerModel::dropMimeData(const QMimeData* mimeData,
                                  Qt::DropAction action,
                                  int row, int column, const QModelIndex& parent)
{
    if(action == Qt::IgnoreAction)
    {
        return true;
    }

    if(action != Qt::MoveAction && action != Qt::CopyAction)
    {
        return false;
    }

    if(! mimeData || (! mimeData->hasFormat(iscore::mime::device()) && ! mimeData->hasFormat(iscore::mime::address())))
    {
        return false;
    }

    QModelIndex parentIndex; //invalid
    Device::Node* parentNode = &m_rootNode;
    QString mimeType = iscore::mime::device();

    if(mimeData->hasFormat(iscore::mime::address()))
    {
        parentIndex = parent;
        parentNode = &nodeFromModelIndex(parent);
        mimeType = iscore::mime::address();

        if(parentNode == &m_rootNode)
        {
            return false;
        }
    }
    else
    {
        ISCORE_ASSERT(mimeData->hasFormat(iscore::mime::device()));
        ISCORE_ASSERT(mimeType == iscore::mime::device());
    }

    if(parentNode)
    {
        // Note : when dropping a device,
        // if there is an existing device that would use the same ports, etc.
        // we have to open a dialog to change the device settings.

        Deserializer<JSONObject> deser{QJsonDocument::fromJson(mimeData->data(mimeType)).object()};
        Device::Node n;
        deser.writeTo(n);

        if(mimeType == iscore::mime::device())
        {
            ISCORE_ASSERT(n.is<Device::DeviceSettings>());

            bool deviceOK = checkDeviceInstantiatable(n.get<Device::DeviceSettings>());
            if(!deviceOK)
            {
                // We ask the user to fix the incompatibilities by himself.
                DeviceEditDialog dial{
                    m_devicePlugin.context().app.components.factory<Device::DynamicProtocolList>(),
                            QApplication::activeWindow()};
                if(!tryDeviceInstantiation(n.get<Device::DeviceSettings>(), dial))
                    return false;
            }

            // Perform the loading
            auto cmd = new Command::LoadDevice{
                       deviceModel(),
                       std::move(n)};

            m_cmdQ->redoAndPush(cmd);
        }

        return true;
    }

    return false;
}

QModelIndex
DeviceExplorerModel::convertPathToIndex(const Device::NodePath& path)
{
    QModelIndex iter;
    const int pathSize = path.size();

    for(int i = 0; i < pathSize; ++i)
    {
        iter = index(path.at(i), 0, iter);
    }

    return iter;
}

void
DeviceExplorerModel::debug_printPath(const Device::NodePath& path)
{
    const int pathSize = path.size();

    for(int i = 0; i < pathSize; ++i)
    {
        std::cerr << path.at(i) << " ";
    }

    std::cerr << "\n";
}

void
DeviceExplorerModel::debug_printIndexes(const QModelIndexList& indexes)
{
    std::cerr << "indexes: " << indexes.size() << " nodes: \n";
    foreach(const QModelIndex & index, indexes)
    {
        if(index.isValid())
        {
            std::cerr << " index.row=" << index.row() << " col=" << index.column() << " ";
            Device::Node* n = &nodeFromModelIndex(index);
            std::cerr << " n=" << n << " ";
            Device::Node* parent = n->parent();

            if(n == &m_rootNode)
            {
                std::cerr << " rootNode parent=" << parent << "\n";
            }
            else
            {
                std::cerr << " n->name=" << n->displayName().toStdString();
                std::cerr << " parent=" << parent;
                std::cerr << " parent->name=" << parent->displayName().toStdString() << "\n";
            }

        }
        else
        {
            std::cerr << " invalid index \n";
        }
    }
}

State::MessageList getSelectionSnapshot(DeviceExplorerModel& model)
{
    // Filter
    auto uniqueNodes = model.uniqueSelectedNodes(model.selectedIndexes());

    // Recursive refresh
    model.deviceModel().updateProxy.refreshRemoteValues(uniqueNodes.parents);

    // Conversion
    State::MessageList messages;
    for(const auto& node : uniqueNodes.parents)
    {
        Device::parametersList(*node, messages);
    }
    for(const auto& node : uniqueNodes.messages)
    {
        messages += Device::message(*node);
    }

    return messages;
}



DeviceExplorerModel* try_deviceExplorerFromObject(const QObject& obj)
{
    auto plug = iscore::IDocument::documentContext(obj).findPlugin<DeviceDocumentPlugin>();
    if(plug)
        return &plug->explorer;
    return nullptr;
}

DeviceExplorerModel& deviceExplorerFromObject(const QObject& obj)
{
    auto expl = try_deviceExplorerFromObject(obj);
    ISCORE_ASSERT(expl);
    return *expl;
}

DeviceExplorerModel* try_deviceExplorerFromContext(const iscore::DocumentContext& ctx)
{
    auto plug = ctx.findPlugin<DeviceDocumentPlugin>();
    if(plug)
        return &plug->explorer;
    return nullptr;
}

DeviceExplorerModel& deviceExplorerFromContext(const iscore::DocumentContext& ctx)
{
    auto expl = try_deviceExplorerFromContext(ctx);
    ISCORE_ASSERT(expl);
    return *expl;
}
}
