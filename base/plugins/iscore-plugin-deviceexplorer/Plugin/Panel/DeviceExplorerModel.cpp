#include "DeviceExplorerModel.hpp"
#include "DeviceExplorerView.hpp"
#include "DeviceExplorerCommandCreator.hpp"

#include "Commands/Insert.hpp"
#include "Commands/EditData.hpp"
#include "Commands/Add/LoadDevice.hpp"
#include "Commands/Update/UpdateAddressSettings.hpp"

#include "DeviceExplorerMimeTypes.hpp"
#include "DocumentPlugin/DeviceDocumentPlugin.hpp"
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/command/CommandStack.hpp>

#include <Singletons/DeviceExplorerInterface.hpp>
#include <State/State.hpp>
#include <State/StateMimeTypes.hpp>
#include <QJsonDocument>
#include <iostream>
#include <QMimeData>
#include "Singletons/SingletonProtocolList.hpp"
#include <DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp>
#include "Widgets/DeviceEditDialog.hpp"
#include <DeviceExplorer/ItemModels/NodeDisplayMethods.hpp>
#include <QApplication>

using namespace DeviceExplorer::Command;
using namespace iscore;


static const QMap<DeviceExplorerModel::Column, QString> HEADERS{
    {DeviceExplorerModel::Column::Name, QObject::tr("Address")},
    {DeviceExplorerModel::Column::Value, QObject::tr("Value")},
    {DeviceExplorerModel::Column::IOType, QObject::tr("I/O")},
    {DeviceExplorerModel::Column::Min, QObject::tr("Min")},
    {DeviceExplorerModel::Column::Max, QObject::tr("Max")}
};

DeviceExplorerModel::DeviceExplorerModel(
        DeviceDocumentPlugin* plug,
        QObject* parent)
    : QAbstractItemModel{parent},
      m_lastCutNodeIsCopied{false},
      m_devicePlugin{plug},
      m_rootNode{plug->rootNode()},
      m_cmdQ{nullptr}
{
    this->setObjectName("DeviceExplorerModel");
    m_devicePlugin->updateProxy.m_deviceExplorer = this;

    beginResetModel();
    endResetModel();


    m_cmdCreator = new DeviceExplorerCommandCreator{this};
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
    return *m_devicePlugin;
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

void
DeviceExplorerModel::setCommandQueue(iscore::CommandStack* q)
{
    m_cmdQ = q;
    m_cmdCreator->setCommandQueue(q);
}

QStringList
DeviceExplorerModel::getColumns() const
{
    return HEADERS.values();
}

int DeviceExplorerModel::addDevice(const Node& deviceNode)
{
    int row = m_rootNode.childCount();
    QModelIndex parent; //invalid

    beginInsertRows(parent, row, row);
    rootNode().push_back(deviceNode);
    endInsertRows();

    return row;
}

int DeviceExplorerModel::addDevice(Node&& deviceNode)
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
        const DeviceSettings& dev)
{
    for(int i = 0; i < m_rootNode.childCount(); i++)
    {
        auto n = &m_rootNode.childAt(i);
        if(n->get<iscore::DeviceSettings>().name == name)
        {
            QModelIndex index = createIndex(i, 0, n->parent());

            QModelIndex changedTopLeft = index;
            QModelIndex changedBottomRight = index;

            n->set(dev);

            emit dataChanged(changedTopLeft, changedBottomRight);
            return;
        }
    }
}

void DeviceExplorerModel::addAddress(
        Node* parentNode,
        const iscore::AddressSettings &addressSettings)
{
    ISCORE_ASSERT(parentNode);
    ISCORE_ASSERT(parentNode != &m_rootNode);

    int row = parentNode->childCount(); //insert as last child

    Node* grandparent = parentNode->parent();
    ISCORE_ASSERT(grandparent);
    int rowParent = grandparent->indexOfChild(parentNode);
    QModelIndex parentIndex = createIndex(rowParent, 0, parentNode);

    beginInsertRows(parentIndex, row, row);

    parentNode->emplace(parentNode->begin() + row, addressSettings, parentNode);

    endInsertRows();
}

void DeviceExplorerModel::updateAddress(
        Node *node,
        const AddressSettings &addressSettings)
{
    ISCORE_ASSERT(node);
    ISCORE_ASSERT(node != &m_rootNode);

    node->set(addressSettings);

    // OPTIMIZEME
    QModelIndex nodeIndex = convertPathToIndex(iscore::NodePath(*node));

    emit dataChanged(
                createIndex(nodeIndex.row(), 0, node->parent()),
                createIndex(nodeIndex.row(), HEADERS.count(), node->parent()));
}

void DeviceExplorerModel::updateValue(iscore::Node* n, const iscore::Value& v)
{
    n->get<iscore::AddressSettings>().value = v;

    // OPTIMIZEME
    QModelIndex nodeIndex = convertPathToIndex(iscore::NodePath(*n));

    auto idx = createIndex(nodeIndex.row(), 1, n->parent());
    emit dataChanged(idx, idx);
}

void DeviceExplorerModel::removeNode(
        iscore::Node::const_iterator node)
{
    ISCORE_ASSERT(!node->is<InvisibleRootNodeTag>());

    if(!node->is<DeviceSettings>())
    {
        Node* parent = node->parent();
        ISCORE_ASSERT(parent != &m_rootNode);
        Node* grandparent = parent->parent();
        ISCORE_ASSERT(grandparent);
        int rowParent = grandparent->indexOfChild(parent);
        QModelIndex parentIndex = createIndex(rowParent, 0, parent);

        int row = parent->indexOfChild(&*node);

        beginRemoveRows(parentIndex, row, row);
        parent->removeChild(node);
        endRemoveRows();
    }
    else
    {
        int row = rootNode().indexOfChild(&*node);

        beginRemoveRows(QModelIndex(), row, row);
        m_rootNode.removeChild(node);
        endRemoveRows();
    }
}

bool DeviceExplorerModel::checkDeviceInstantiatable(
        iscore::DeviceSettings& n)
{
    // Request from the protocol factory the protocol to see
    // if it is compatible.
    auto prot = SingletonProtocolList::instance().protocol(n.protocol);
    if(!prot)
        return false;

    // Look for other childs in the same protocol.
    for(const auto& child : rootNode())
    {
        ISCORE_ASSERT(child.is<DeviceSettings>());
        if(child.get<DeviceSettings>().protocol == n.protocol)
        {
            if(!prot->checkCompatibility(n, child.get<DeviceSettings>()))
            {
                // Open a device edit window
                // it should take care of incompatibility with the other
                // devices
                DeviceEditDialog dial(QApplication::activeWindow());
                dial.setSettings(n);
                bool ret = dial.exec();
                if(!ret)
                    return false;

                n = dial.getSettings();
                return true;
            }
        }
    }

    return true;
}


QModelIndex
DeviceExplorerModel::index(int row, int column, const QModelIndex& parent) const
{
    if(row < 0 || column < 0 || column >= (int)Column::Count)
    {
        return {};
    }

    Node* parentNode = nodeFromModelIndex(parent);
    if(! parentNode)
        return {};

    if(!parentNode->hasChild(row))
        return {};

    return createIndex(row, column, &parentNode->childAt(row));
}

Node*
DeviceExplorerModel::nodeFromModelIndex(const QModelIndex& index) const
{
    return index.isValid()
            ? static_cast<iscore::Node*>(index.internalPointer())
            : &m_rootNode;
}

QModelIndex
DeviceExplorerModel::parent(const QModelIndex& child) const
{
    Node* node = nodeFromModelIndex(child);

    if(! node)
    {
        return QModelIndex();
    }

    Node* parentNode = node->parent();

    if(! parentNode)
    {
        return QModelIndex();
    }

    Node* grandparentNode = parentNode->parent();

    if(! grandparentNode)
    {
        return QModelIndex();
    }

    const int rowParent = grandparentNode->indexOfChild(parentNode);  //(return -1 if not found)
    assert(rowParent != -1);
    return createIndex(rowParent, 0, parentNode);
}

int
DeviceExplorerModel::rowCount(const QModelIndex& parent) const
{
    if(parent.column() > 0)
    {
        return 0;
    }

    Node* parentNode = nodeFromModelIndex(parent);

    if(! parentNode)
    {
        return 0;
    }

    return parentNode->childCount();
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

QVariant DeviceExplorerModel::getData(iscore::NodePath node, Column column, int role)
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

    Node* node = nodeFromModelIndex(index);
    if(! node)
    {
        return QVariant();
    }

    const auto& n = *node;
    switch((Column)col)
    {
        case Column::Name:
            return nameColumnData(n, role);

        case Column::Value:
            return valueColumnData(n, role);

        case Column::IOType:
            return IOTypeColumnData(n, role);

        case Column::Min:
            return minColumnData(n, role);

        case Column::Max:
            return maxColumnData(n, role);

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
        Node* n = nodeFromModelIndex(index);
        assert(n);

        if(n->isSelectable())
        {
            f |= Qt::ItemIsSelectable;
        }

        //we allow drag'n drop only from the name column
        if(index.column() == (int)Column::Name)
        {
            f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
        }


        if(n->isEditable())
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
bool
DeviceExplorerModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(! index.isValid())
    {
        return false;
    }

    auto n = nodeFromModelIndex(index);
    if(! n)
    {
        return false;
    }

    if(n->is<DeviceSettings>())
    {
        return false;
    }

    auto col = DeviceExplorerModel::Column(index.column());

    if(role == Qt::EditRole)
    {
        if(col == Column::Value)
        {
            // In this case we don't make a command, but we directly push the
            // new value.
            QVariant copy = value;
            auto res = copy.convert(n->get<iscore::AddressSettings>().value.val.type());
            if(res)
            {
                n->get<iscore::AddressSettings>().value.val = copy;
                // Note : if we want to disable remote updating, we have to do it
                // here (e.g. if this becomes a settings)
                m_devicePlugin->updateProxy.updateRemoteValue(iscore::address(*n), iscore::Value::fromVariant(copy));

                return true;
            }

            return false;
        }
        else
        {
            // Here we make a command because we change the structure of the tree.
            auto settings = n->get<iscore::AddressSettings>();
            if(col == Column::Name)
            {
                const QString s = value.toString();
                if(! s.isEmpty())
                {
                    settings.name = s;
                }
            }
            else if(col == Column::IOType)
            {
                // TODO Harmonize this with IOTypeDelegate to prevent the use of this map
                settings.ioType = iscore::IOTypeStringMap().key(value.value<QString>());
            }

            if(settings != n->get<iscore::AddressSettings>())
            {
                // We changed
                m_cmdQ->redoAndPush(new DeviceExplorer::Command::UpdateAddressSettings{
                                        iscore::IDocument::path(this->deviceModel()),
                                        iscore::NodePath{*n},
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
        const iscore::NodePath &path,
        DeviceExplorerModel::Column column,
        const QVariant &value,
        int role)
{
    QModelIndex nodeIndex = convertPathToIndex(path);
    Node* node = nodeFromModelIndex(nodeIndex);

    QModelIndex index = createIndex(nodeIndex.row(), (int)column, node->parent());

    QModelIndex changedTopLeft = index;
    QModelIndex changedBottomRight = index;

    if(node->is<DeviceSettings>())
        return;

    if(role == Qt::EditRole)
    {
        if(index.column() == (int)Column::Name)
        {
            const QString s = value.toString();

            if(! s.isEmpty())
            {
                node->get<iscore::AddressSettings>().name = s;
            }
        }
        else if(index.column() == (int)Column::IOType)
        {
            node->get<iscore::AddressSettings>().ioType = IOTypeStringMap().key(value.toString());
        }
        else if(index.column() == (int)Column::Value)
        {
            node->get<iscore::AddressSettings>().value.val = value;
        }
    }

    emit dataChanged(changedTopLeft, changedBottomRight);
}


QModelIndex
DeviceExplorerModel::bottomIndex(const QModelIndex& index) const
{
    Node* node = nodeFromModelIndex(index);

    if(! node)
    {
        return index;
    }

    if(! node->hasChildren())
    {
        return index;
    }

    return bottomIndex(
                createIndex(
                    node->childCount() - 1,
                    index.column(),
                    &node->childAt(node->childCount() - 1)));
}

bool
DeviceExplorerModel::isDevice(QModelIndex index) const
{
    if(!index.isValid())
    {
        return false;
    }

    Node* n = nodeFromModelIndex(index);
    ISCORE_ASSERT(n);
    return n->is<DeviceSettings>();
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


DeviceExplorer::Result
DeviceExplorerModel::cut_aux(const QModelIndex& index)
{
    ISCORE_TODO;
    return false;
    /*
    if(!index.isValid())
    {
        return DeviceExplorer::Result(false, index);
    }


    Node* cutNode = nodeFromModelIndex(index);
    ISCORE_ASSERT(cutNode);

    const bool cutNodeIsDevice = cutNode->is<DeviceSettings>();


    if(! m_cutNodes.isEmpty() && m_lastCutNodeIsCopied)
    {
        //necessary to avoid that several successive copies fill m_cutNodes (copy is not a command)
        Node* prevCopiedNode = m_cutNodes.pop().first;
        delete prevCopiedNode;
    }

    m_cutNodes.push(CutElt(cutNode, cutNodeIsDevice));
    m_lastCutNodeIsCopied = false;

    Node* parent = cutNode->parent();
    ISCORE_ASSERT(parent);

    int row = parent->indexOfChild(cutNode);
    ISCORE_ASSERT(row == index.row());

    beginRemoveRows(index.parent(), row, row);

    Node* child = parent->takeChild(row);
    ISCORE_ASSERT(child == cutNode);

    endRemoveRows();

    //TODO: we should emit a signal to indicate that a paste is now possible !?!

    if(row > 0)
    {
        --row;
        return DeviceExplorer::Result(createIndex(row, 0, &parent->childAt(row)));
    }

    if(parent != &m_rootNode)
    {
        Node* grandParent = parent->parent();
        ISCORE_ASSERT(grandParent);
        return DeviceExplorer::Result(createIndex(grandParent->indexOfChild(parent), 0, parent));
    }

    return DeviceExplorer::Result(QModelIndex());
    */
}


/*
  Paste behavior.

  We always paste as a sibling of the selected index.
  We provide two methods pasteBefore() & pasteAfter().

  pasteAfter() inserts the pasted node after the selected node in its parent list.
  Thus it means that we can never paste the item as first child.

  pasteBefore() inserts the pasted node before the selected node in its parent list.
  Thus we can paste as first child but we can not paste as last child.

*/

DeviceExplorer::Result
DeviceExplorerModel::paste_aux(const QModelIndex& index, bool after)
{
    ISCORE_TODO;
    return {false};
    /*
    if(m_cutNodes.isEmpty())
    {
        return DeviceExplorer::Result(false, index);
    }

    if(! index.isValid() && ! m_cutNodes.top().second)  //we can not pass addresses at top level
    {
        return DeviceExplorer::Result(false, index);
    }


    //REM: we always paste as sibling


    Node* parent = nullptr;
    int row = 0;
    QModelIndex parentIndex;

    const bool cutNodeIsDevice = m_cutNodes.top().second;
    Node* cutNode = m_cutNodes.top().first;
    m_cutNodes.pop();

    if(index.isValid())
    {
        Node* n = nodeFromModelIndex(index);
        ISCORE_ASSERT(n);

        parent = n->parent();
        ISCORE_ASSERT(parent);

        parentIndex = index.parent();

        if(cutNodeIsDevice)
        {
            //we can only paste devices at the top-level
            while(parent != &m_rootNode)
            {
                ISCORE_ASSERT(parent);
                n = parent;
                parent = parent->parent();
            }

            ISCORE_ASSERT(parent->indexOfChild(n) != -1);

            parentIndex = QModelIndex(); //invalid on purpose

        }

        row = parent->indexOfChild(n) + (after ? 1 : 0);

    }
    else
    {
        ISCORE_ASSERT(! index.isValid() && cutNodeIsDevice);
        parent = &m_rootNode;
        row = m_rootNode.childCount();
        parentIndex = QModelIndex(); //invalid on purpose
    }

    ISCORE_ASSERT(parent);

    ISCORE_ASSERT(cutNode);

    beginInsertRows(parentIndex, row, row);

    parent->insertChild(row, cutNode);

    Node* child = cutNode;

    endInsertRows();

    return DeviceExplorer::Result(createIndex(row, 0, child));
    */
}

DeviceExplorer::Result
DeviceExplorerModel::pasteAfter_aux(const QModelIndex& index)
{
    return paste_aux(index, true);
}

DeviceExplorer::Result
DeviceExplorerModel::pasteBefore_aux(const QModelIndex& index)
{
    return paste_aux(index, false);
}

bool
DeviceExplorerModel::moveRows(const QModelIndex& srcParentIndex, int srcRow, int count,
                              const QModelIndex& dstParentIndex, int dstRow)
{
    ISCORE_TODO;
    return false;
    /*
    if(!srcParentIndex.isValid() || !dstParentIndex.isValid())
    {
        return false;
    }

    if(srcParentIndex == dstParentIndex && (srcRow <= dstRow && dstRow <= srcRow + count - 1))
    {
        return false;
    }

    Node* srcParent = nodeFromModelIndex(srcParentIndex);
    ISCORE_ASSERT(srcParent);

    if(srcRow + count > srcParent->childCount())
    {
        return false;
    }


    beginMoveRows(srcParentIndex, srcRow, srcRow + count - 1, dstParentIndex, dstRow);

    if(srcParentIndex == dstParentIndex)
    {
        //move up or down inside the same parent

        Node* parent = srcParent;
        ISCORE_ASSERT(parent);

        if(srcRow > dstRow)
        {
            for(int i = 0; i < count; ++i)
            {
                Node* n = parent->takeChild(srcRow + i);
                parent->insertChild(dstRow + i, n);
            }
        }
        else
        {
            ISCORE_ASSERT(srcRow < dstRow);

            for(int i = 0; i < count; ++i)
            {
                Node* n = parent->takeChild(srcRow);
                parent->insertChild(dstRow - 1, n);
            }
        }

    }
    else
    {
        //different parents

        Node* dstParent = nodeFromModelIndex(dstParentIndex);
        ISCORE_ASSERT(dstParent);
        ISCORE_ASSERT(dstParent != srcParent);

        for(int i = 0; i < count; ++i)
        {
            Node* n = srcParent->takeChild(srcRow);
            dstParent->insertChild(dstRow + i, n);
        }

    }

    endMoveRows();

    return true;
    */
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


/**
 * @brief filterUniqueParents
 * @param nodes A list of nodes
 * @return Another list of nodes
 *
 * This function filters a list of node
 * by only keeping the nodes that had no ancestor.
 *
 * e.g. given the tree :
 *
 * a -> b -> d
 *        -> e
 *   -> c
 * f -> g
 *
 * If the input consists of b, d, the output will be b.
 * If the input consists of a, b, d, f, the output will be a, f.
 * If the input consists of d, e, the output will be d, e.
 *
 * TESTME
 */
static QList<iscore::Node*> filterUniqueParents(const QList<iscore::Node*>& nodes)
{
    // TODO optimizeme this horrible lazy algorithm.
    auto nodes_cpy = nodes;
    QList<iscore::Node*> cleaned_nodes;

    // Only copy the index if it none of its parents
    // except the invisible root are in the list.
    for(auto n : nodes)
    {
        if(std::any_of(nodes_cpy.begin(), nodes_cpy.end(),
                       [&] (iscore::Node* other)
        {
            if(other == n)
                return false;

            return isAncestor(*other, n);
           }))
        {
            nodes_cpy.removeOne(n);
        }
        else
        {
            cleaned_nodes.append(n);
        }
    }

    return cleaned_nodes;
}


//method called when a drag is initiated
QMimeData*
DeviceExplorerModel::mimeData(const QModelIndexList& indexes) const
{
    QList<iscore::Node*> nodes;
    std::transform(indexes.begin(), indexes.end(), std::back_inserter(nodes),
                   [&] (const QModelIndex& idx) {
        return nodeFromModelIndex(idx);
    });

    nodes.removeAll(&m_rootNode);

    auto uniqueNodes = filterUniqueParents(nodes);

    QMimeData* mimeData = new QMimeData;

    // The "Nodes" part.
    mimeData->setData(
                iscore::mime::nodes(),
                QJsonDocument(toJsonArray(nodes)).toJson(QJsonDocument::Indented));

    // The "MessagesList" part.
    MessageList messages;
    for(const auto& node : uniqueNodes)
    {
        messageList(*node, messages);
    }
    if(!messages.empty())
    {

        Serializer<JSONObject> ser;
        ser.readFrom(messages);

        // TODO add a nodes() mime type which contains an array of nodes.
        // The mime data should also transmit the root address for
        // each node in this case.
        mimeData->setData(iscore::mime::messagelist(),
                          QJsonDocument(ser.m_obj).toJson(QJsonDocument::Indented));

    }

    if(messages.empty() && nodes.empty())
    {
        delete mimeData;
        return nullptr;
    }

    return mimeData;
}


bool
DeviceExplorerModel::canDropMimeData(const QMimeData* mimeData,
                                     Qt::DropAction action,
                                     int /*row*/, int /*column*/, const QModelIndex& parent) const
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


    Node* parentNode = nodeFromModelIndex(parent);

    if(mimeData->hasFormat(iscore::mime::address()))
    {
        if(parentNode == &m_rootNode)
        {
            return false;
        }
    }
    else
    {
        ISCORE_ASSERT(mimeData->hasFormat(iscore::mime::device()));

        if(parentNode != &m_rootNode)
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
    Node* parentNode = &m_rootNode;
    QString mimeType = iscore::mime::device();

    if(mimeData->hasFormat(iscore::mime::address()))
    {
        parentIndex = parent;
        parentNode = nodeFromModelIndex(parent);
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

        if(row == -1)
        {
            row = parentNode->childCount();
        }

        Deserializer<JSONObject> deser{QJsonDocument::fromJson(mimeData->data(mimeType)).object()};
        Node n;
        deser.writeTo(n);

        if(mimeType == iscore::mime::device())
        {
            ISCORE_ASSERT(n.is<DeviceSettings>());

            // Edit the device settings if necessary
            bool deviceOK = checkDeviceInstantiatable(n.get<DeviceSettings>());
            if(!deviceOK)
                return false;

            // Perform the loading
            auto cmd = new LoadDevice{
                    iscore::IDocument::path(deviceModel()),
                    std::move(n)};

            m_cmdQ->redoAndPush(cmd);
        }

        return true;
    }

    return false;
}

void
DeviceExplorerModel::setCachedResult(DeviceExplorer::Result r)
{
    m_cmdCreator->setCachedResult(r);
}

QModelIndex
DeviceExplorerModel::convertPathToIndex(const iscore::NodePath& path)
{
    QModelIndex iter;
    const int pathSize = path.size();

    for(int i = 0; i < pathSize; ++i)
    {
        iter = index(path.at(i), 0, iter);
    }

    return iter;
}

DeviceExplorerCommandCreator *DeviceExplorerModel::cmdCreator()
{
    return m_cmdCreator;
}

void
DeviceExplorerModel::debug_printPath(const iscore::NodePath& path)
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
            Node* n = nodeFromModelIndex(index);

            if(n)
            {
                std::cerr << " n=" << n << " ";
                Node* parent = n->parent();

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
                std::cerr << " invalid node\n";
            }
        }
        else
        {
            std::cerr << " invalid index \n";
        }
    }
}
