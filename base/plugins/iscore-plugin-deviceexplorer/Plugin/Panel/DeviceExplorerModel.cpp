#include "DeviceExplorerModel.hpp"
#include "DeviceExplorerView.hpp"
#include <DeviceExplorer/Node/Node.hpp>
#include "DeviceExplorerCommandCreator.hpp"

#include <iostream>

#include <QMimeData>

#include "Commands/Insert.hpp"
#include "Commands/EditData.hpp"

#include <core/command/CommandStack.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <QJsonDocument>
#include "DocumentPlugin/DeviceDocumentPlugin.hpp"
#include <State/StateMimeTypes.hpp>
#include "DeviceExplorerMimeTypes.hpp"

using namespace DeviceExplorer::Command;
using namespace iscore;


static const QMap<DeviceExplorerModel::Column, QString> HEADERS{
    {DeviceExplorerModel::Column::Name, QObject::tr("Address")},
    {DeviceExplorerModel::Column::Value, QObject::tr("Value")},
    {DeviceExplorerModel::Column::IOType, QObject::tr("I/O")},
    {DeviceExplorerModel::Column::Min, QObject::tr("Min")},
    {DeviceExplorerModel::Column::Max, QObject::tr("Max")},
    {DeviceExplorerModel::Column::Priority, QObject::tr("Priority")},
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

DeviceDocumentPlugin *DeviceExplorerModel::deviceModel() const
{
    return m_devicePlugin;
}

QModelIndexList DeviceExplorerModel::selectedIndexes() const
{
    return m_view->selectedIndexes();
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

int DeviceExplorerModel::addDevice(Node* deviceNode)
{
    int row = m_rootNode.childCount();
    QModelIndex parent; //invalid

    beginInsertRows(parent, row, row);
    m_rootNode.insertChild(row, deviceNode);
    endInsertRows();

    return row;
}

Node* DeviceExplorerModel::addAddress(
        Node* parentNode,
        const iscore::AddressSettings &addressSettings)
{
    Q_ASSERT(parentNode);
    Q_ASSERT(parentNode != &m_rootNode);

    int row = parentNode->childCount(); //insert as last child

    Node* grandparent = parentNode->parent();
    Q_ASSERT(grandparent);
    int rowParent = grandparent->indexOfChild(parentNode);
    QModelIndex parentIndex = createIndex(rowParent, 0, parentNode);

    beginInsertRows(parentIndex, row, row);

    auto node = new Node{addressSettings};
    parentNode->insertChild(row, node);

    endInsertRows();

    return node;
}

void DeviceExplorerModel::addAddress(Node *parentNode, Node *node, int row)
{
    Q_ASSERT(parentNode);
    Q_ASSERT(parentNode != &m_rootNode);

    if (row == -1)
    {
        row = parentNode->childCount(); //insert as last child
    }

    Node* grandparent = parentNode->parent();
    Q_ASSERT(grandparent);
    int rowParent = grandparent->indexOfChild(parentNode);
    QModelIndex parentIndex = createIndex(rowParent, 0, parentNode);

    beginInsertRows(parentIndex, row, row);

    parentNode->insertChild(row, node);

    endInsertRows();
}

void DeviceExplorerModel::removeNode(Node* node)
{
    Q_ASSERT(node);
    Q_ASSERT(node != &m_rootNode);

    Node* parent = node->parent();

    Q_ASSERT(parent != &m_rootNode);
    Node* grandparent = parent->parent();
    Q_ASSERT(grandparent);
    int rowParent = grandparent->indexOfChild(parent);
    QModelIndex parentIndex = createIndex(rowParent, 0, parent);

    int row = parent->indexOfChild(node);

    beginRemoveRows(parentIndex, row, row);
    parent->removeChild(node);
    endRemoveRows();
}


QModelIndex
DeviceExplorerModel::index(int row, int column, const QModelIndex& parent) const
{
    if(row < 0 || column < 0 || column >= (int)Column::Count)
    {
        return {};
    }

    Node* parentNode = nodeFromModelIndex(parent);
    if(! parentNode) return {};

    Node* childNode = parentNode->childAt(row);  //value() return 0 if out of bounds
    if(! childNode) return {};

    return createIndex(row, column, childNode);
}

Node*
DeviceExplorerModel::nodeFromModelIndex(const QModelIndex& index) const
{
    return index.isValid()
            ? static_cast<Node*>(index.internalPointer())
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

QVariant DeviceExplorerModel::getData(NodePath node, Column column, int role)
{
    QModelIndex index = createIndex(convertPathToIndex(node).row(), (int)column, node.toNode(&rootNode())->parent());
    return data(index, role);
}

// must return an invalid QVariant for cases not handled
QVariant
DeviceExplorerModel::data(const QModelIndex& index, int role) const
{
    //if (role != Qt::DisplayRole)
    //return QVariant();

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

    switch((Column)col)
    {
        case Column::Name:
        {
            if(node->isDevice())
            {
                if(role == Qt::DisplayRole || role == Qt::EditRole)
                {
                    return node->deviceSettings().name;
                }
            }
            else
            {
                if(role == Qt::DisplayRole || role == Qt::EditRole)
                {
                    return node->displayName();
                }
                else if(role == Qt::FontRole)
                {
                    const IOType ioType = node->addressSettings().ioType;

                    if(ioType == IOType::In || ioType == IOType::Out)
                    {
                        QFont f; // = QAbstractItemModel::data(index, role); //B: how to get current font ?
                        f.setItalic(true);
                        return f;
                    }
                }
                else if(role == Qt::ForegroundRole)
                {
                    const IOType ioType = node->addressSettings().ioType;

                    if(ioType == IOType::In || ioType == IOType::Out)
                    {
                        return QBrush(Qt::black);
                    }
                }
            }

        }
            break;

        case Column::Value:
        {
            if(node->isDevice())
                return {};

            if(role == Qt::DisplayRole || role == Qt::EditRole)
            {
                const auto& val = node->addressSettings().value;
                if(val.val.canConvert<QVariantList>())
                {
                    return val.val.toStringList().join(", ");
                }

                return val.val.toString();
            }
            else if(role == Qt::ForegroundRole)
            {
                const IOType ioType = node->addressSettings().ioType;

                if(ioType == IOType::In || ioType == IOType::Out)
                {
                    return QBrush(Qt::black);
                }
            }
        }
            break;

        case Column::IOType:
        {
            if(node->isDevice())
                return {};

            if(role == Qt::DisplayRole || role == Qt::EditRole)
            {
                switch(node->addressSettings().ioType)
                {
                    case IOType::In:
                        return QString("<-");

                    case IOType::Out:
                        return QString("->");

                    case IOType::InOut:
                        return QString("<->");

                    default:
                        return {};
                }
            }
        }
            break;

        case Column::Min:
        {
            if(node->isDevice())
                return {};

            if(role == Qt::DisplayRole || role == Qt::EditRole)
            {
                return node->addressSettings().domain.min.val;
            }
        }
            break;

        case Column::Max:
        {
            if(node->isDevice())
                return {};

            if(role == Qt::DisplayRole || role == Qt::EditRole)
            {
                return node->addressSettings().domain.max.val;
            }
        }
            break;

        case Column::Priority:
        {
            if(node->isDevice())
                return {};

            if(role == Qt::DisplayRole || role == Qt::EditRole)
            {
                return node->addressSettings().priority;
            }
        }
            break;

        default :
            assert(false);
            return QString();
    }



    return QVariant();
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

    return QVariant();
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
            //std::cerr<<"DeviceExplorerModel::flags "<<n->name().toStdString()<<" ioType="<<(int)n->ioType()<<" Qt::ItemIsEditable\n";

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

    if(n->isDevice())
    {
        return false;
    }

    bool changed = false;
    auto col = DeviceExplorerModel::Column(index.column());

    if(role == Qt::EditRole)
    {
        if(col == Column::Name)
        {
            const QString s = value.toString();

            if(! s.isEmpty())
            {
                m_cmdQ->redoAndPush(new EditData{iscore::IDocument::path(this), NodePath{index}, col, value, role});
                changed = true;
            }
        }
        else if(col == Column::IOType)
        {
            m_cmdQ->redoAndPush(new EditData{iscore::IDocument::path(this), NodePath{index}, col, value, role});
            changed = true;
        }
        else if(col == Column::Value)
        {
            QVariant copy = value;
            auto res = copy.convert(n->addressSettings().value.val.type());
            if(res)
            {
                m_cmdQ->redoAndPush(new EditData{iscore::IDocument::path(this),
                                                 NodePath{index},
                                                 col,
                                                 copy,
                                                 role});
                changed = true;
            }
        }
    }

    return changed; //false;
}

bool
DeviceExplorerModel::setHeaderData(int, Qt::Orientation, const QVariant&, int)
{
    return false; //we prevent editing the (column) headers
}

void DeviceExplorerModel::editData(const NodePath &path, DeviceExplorerModel::Column column, const QVariant &value, int role)
{
    QModelIndex nodeIndex = convertPathToIndex(path);
    Node* node = nodeFromModelIndex(nodeIndex);

    QModelIndex index = createIndex(nodeIndex.row(), (int)column, node->parent());

    QModelIndex changedTopLeft = index;
    QModelIndex changedBottomRight = index;

    if(node->isDevice())
        return;

    if(role == Qt::EditRole)
    {
        if(index.column() == (int)Column::Name)
        {
            const QString s = value.toString();

            if(! s.isEmpty())
            {
                node->addressSettings().name = s;
            }
        }

        if(index.column() == (int)Column::IOType)
        {
            node->addressSettings().ioType = IOTypeStringMap().key(value.toString());
        }

        if(index.column() == (int)Column::Value)
        {
            node->addressSettings().value.val = value;
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

    return bottomIndex(createIndex(node->childCount() - 1, index.column(), node->childAt(node->childCount() - 1)));
}

//this method is called (behind the scenes) when there is a drag and drop to delete the original dragged rows once they have been dropped (dropped rows are inserted using insertRows)
bool
DeviceExplorerModel::removeRows(int row, int count, const QModelIndex& parent)
{
    Node* parentNode = parent.isValid() ? nodeFromModelIndex(parent) : &m_rootNode;
    beginRemoveRows(parent, row, row + count - 1);

    for(int i = 0; i < count; ++i)
    {
        Node* n = parentNode->takeChild(row);
        delete n;
    }

    endRemoveRows();

    return true;
}

bool
DeviceExplorerModel::isDevice(QModelIndex index) const
{
    if(!index.isValid())
    {
        return false;
    }

    Node* n = nodeFromModelIndex(index);
    Q_ASSERT(n);
    return n->isDevice();
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
    if(!index.isValid())
    {
        return DeviceExplorer::Result(false, index);
    }


    Node* cutNode = nodeFromModelIndex(index);
    Q_ASSERT(cutNode);

    const bool cutNodeIsDevice = cutNode->isDevice();


    if(! m_cutNodes.isEmpty() && m_lastCutNodeIsCopied)
    {
        //necessary to avoid that several successive copies fill m_cutNodes (copy is not a command)
        Node* prevCopiedNode = m_cutNodes.pop().first;
        delete prevCopiedNode;
    }

    m_cutNodes.push(CutElt(cutNode, cutNodeIsDevice));
    m_lastCutNodeIsCopied = false;

    Node* parent = cutNode->parent();
    Q_ASSERT(parent);

    int row = parent->indexOfChild(cutNode);
    Q_ASSERT(row == index.row());

    beginRemoveRows(index.parent(), row, row);

    #ifndef QT_NO_DEBUG
    Node* child =
        #endif
            parent->takeChild(row);

#ifndef QT_NO_DEBUG
    Q_ASSERT(child == cutNode);
#endif

    endRemoveRows();

    //TODO: we should emit a signal to indicate that a paste is now possible !?!

    if(row > 0)
    {
        --row;
        return createIndex(row, 0, parent->childAt(row));
    }

    if(parent != &m_rootNode)
    {
        Node* grandParent = parent->parent();
        Q_ASSERT(grandParent);
        return createIndex(grandParent->indexOfChild(parent), 0, parent);
    }

    return QModelIndex();
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
        Q_ASSERT(n);

        parent = n->parent();
        Q_ASSERT(parent);

        parentIndex = index.parent();

        if(cutNodeIsDevice)
        {
            //we can only paste devices at the top-level
            while(parent != &m_rootNode)
            {
                Q_ASSERT(parent);
                n = parent;
                parent = parent->parent();
            }

            Q_ASSERT(parent->indexOfChild(n) != -1);

            parentIndex = QModelIndex(); //invalid on purpose

        }

        row = parent->indexOfChild(n) + (after ? 1 : 0);

    }
    else
    {
        Q_ASSERT(! index.isValid() && cutNodeIsDevice);
        parent = &m_rootNode;
        row = m_rootNode.childCount();
        parentIndex = QModelIndex(); //invalid on purpose
    }

    Q_ASSERT(parent);

    Q_ASSERT(cutNode);

    beginInsertRows(parentIndex, row, row);

    parent->insertChild(row, cutNode);

    Node* child = cutNode;

    endInsertRows();

    return createIndex(row, 0, child);
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
    if(!srcParentIndex.isValid() || !dstParentIndex.isValid())
    {
        return false;
    }

    if(srcParentIndex == dstParentIndex && (srcRow <= dstRow && dstRow <= srcRow + count - 1))
    {
        return false;
    }

    Node* srcParent = nodeFromModelIndex(srcParentIndex);
    Q_ASSERT(srcParent);

    if(srcRow + count > srcParent->childCount())
    {
        return false;
    }


    beginMoveRows(srcParentIndex, srcRow, srcRow + count - 1, dstParentIndex, dstRow);

    if(srcParentIndex == dstParentIndex)
    {
        //move up or down inside the same parent

        Node* parent = srcParent;
        Q_ASSERT(parent);

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
            Q_ASSERT(srcRow < dstRow);

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
        Q_ASSERT(dstParent);
        Q_ASSERT(dstParent != srcParent);

        for(int i = 0; i < count; ++i)
        {
            Node* n = srcParent->takeChild(srcRow);
            dstParent->insertChild(dstRow + i, n);
        }

    }

    endMoveRows();

    return true;
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
    return {iscore::mime::device(), iscore::mime::address()}; //TODO: add an xml MimeType to support drop of namespace XML file ?
}

#include <Singletons/DeviceExplorerInterface.hpp>
#include <State/State.hpp>
//method called when a drag is initiated
QMimeData*
DeviceExplorerModel::mimeData(const QModelIndexList& indexes) const
{
    // Algorithm :
    // If there is a single node, we drag it and its children with the full node data
    // If there are multiple nodes, we only drag the messages (not recursively)

    //we drag only one node (and its children recursively).
    if(indexes.count() == 1)
    {
        const QModelIndex index = indexes.at(0);

        Node* n = nodeFromModelIndex(index);

        if(n)
        {
            //m_rootNode not displayed thus should not be draggable
            if(n == &m_rootNode)
                return nullptr;

            Serializer<JSONObject> ser;
            ser.readFrom(*n);
            QMimeData* mimeData = new QMimeData;
            mimeData->setData(
                        n->isDevice() ? iscore::mime::device() : iscore::mime::address(),
                        QJsonDocument(ser.m_obj).toJson(QJsonDocument::Indented));

            if(!n->isDevice())
            {
                MessageList messages;
                messages.push_back(DeviceExplorer::messageFromModelIndex(index));

                State s{messages};
                ser.m_obj = {};
                ser.readFrom(s);
                mimeData->setData(iscore::mime::state(),
                                  QJsonDocument(ser.m_obj).toJson(QJsonDocument::Indented));
            }
            return mimeData;
        }
    }
    else if(indexes.count() > 1)
    {
        MessageList messages;
        for(const auto& index : indexes)
        {
            if(!nodeFromModelIndex(index)->isDevice())
            {
                messages.push_back(DeviceExplorer::messageFromModelIndex(index));
            }
        }

        if(!messages.empty())
        {
            QMimeData* mimeData = new QMimeData;
            State s{messages};

            Serializer<JSONObject> ser;
            ser.readFrom(s);

            mimeData->setData(iscore::mime::state(),
                              QJsonDocument(ser.m_obj).toJson(QJsonDocument::Indented));

            return mimeData;
        }
    }

    return nullptr;
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
        Q_ASSERT(mimeData->hasFormat(iscore::mime::device()));

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
        Q_ASSERT(mimeData->hasFormat(iscore::mime::device()));
        Q_ASSERT(mimeType == iscore::mime::device());
    }

    if(parentNode)
    {
        // Note : when dropping a device,
        // if there is an existing device that would use the same ports, etc.
        // we have to open a dialog to change the device settings.
        /*
        if(row == -1)
        {
            row = parentNode->childCount();
        }

        Deserializer<JSONObject> deser{QJsonDocument::fromJson(mimeData->data(mimeType)).object()};
        Node n;
        deser.writeTo(n);

        auto cmd = new DeviceExplorer::Command::Insert{
                Path(parentNode),
                row,
                std::move(n),
                iscore::IDocument::path(this)};

        m_cmdQ->redoAndPush(cmd);

        if(! m_cmdCreator->m_cachedResult)
        {
            delete m_cmdQ->command(0);
            m_cmdCreator->m_cachedResult = true;
            return false;
        }

        return true;
        */
        return false;
    }

    return false;
}

bool DeviceExplorerModel::insertNode(const QModelIndex& parent, int row, const Node &node)
{
    Node* parentNode = nodeFromModelIndex(parent);

    if(parentNode)
    {
        beginInsertRows(parent, row, row);

        parentNode->insertChild(row, node.clone());

        endInsertRows();

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
DeviceExplorerModel::convertPathToIndex(const NodePath& path)
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
DeviceExplorerModel::debug_printPath(const NodePath& path)
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
