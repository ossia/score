#include "MessageItemModel.hpp"
#include "NodeDisplayMethods.hpp"
#include "Commands/AddMessagesToModel.hpp"
#include <State/MessageListSerialization.hpp>
#include <iscore/document/DocumentInterface.hpp>

using namespace iscore;
MessageItemModel::MessageItemModel(QObject* parent):
    QAbstractItemModel{parent},
    m_rootNode{}
{
    this->setObjectName("MessageItemModel");
}

MessageItemModel &MessageItemModel::operator=(const MessageItemModel & other)
{
    beginResetModel();
    m_rootNode = other.m_rootNode;
    endResetModel();
    return *this;
}

MessageItemModel &MessageItemModel::operator=(const iscore::Node & n)
{
    beginResetModel();
    m_rootNode = n;
    endResetModel();
    return *this;
}

MessageItemModel &MessageItemModel::operator=(iscore::Node && n)
{
    beginResetModel();
    m_rootNode = std::move(n);
    endResetModel();
    return *this;
}

void MessageItemModel::setCommandStack(ptr<CommandStack> stk)
{
    m_stack = stk;
}

iscore::Node *MessageItemModel::nodeFromModelIndex(const QModelIndex &index)
{
    return index.isValid()
            ? static_cast<iscore::Node*>(index.internalPointer())
            : &m_rootNode;
}

const iscore::Node *MessageItemModel::nodeFromModelIndex(const QModelIndex &index) const
{
    return index.isValid()
            ? static_cast<const iscore::Node*>(index.internalPointer())
            : &m_rootNode;
}

void MessageItemModel::removeMessage(iscore::Node *node)
{
    ISCORE_ASSERT(node);

    auto parent = node->parent();
    ISCORE_ASSERT(parent);

    int rowParent = 0;
    if(parent != &m_rootNode)
    {
        auto grandparent = parent->parent();
        ISCORE_ASSERT(grandparent);
        rowParent = grandparent->indexOfChild(parent);
    }

    QModelIndex parentIndex = createIndex(rowParent, 0, parent);

    int row = parent->indexOfChild(node);

    beginRemoveRows(parentIndex, row, row);
    parent->removeChild(parent->cbegin() + row);
    endRemoveRows();
}

void MessageItemModel::mergeMessages(
        const MessageList& messages)
{
    //ISCORE_ASSERT(node);

    // For all the messages, search the ones with same addresses
    // Insert the ones with other addresses
    //auto parent = node->parent();
    //ISCORE_ASSERT(parent);

    //QModelIndex nodeIndex = createIndex(parent->indexOfChild(node), (int)Column::Name, node);
    //node->set(messages);

    //emit dataChanged(nodeIndex, nodeIndex);
}

QModelIndex MessageItemModel::index(int row,
                                  int column,
                                  const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    const iscore::Node* parentItem{};

    if (!parent.isValid())
        parentItem = &m_rootNode; // todo why ?
    else
        parentItem = static_cast<iscore::Node*>(parent.internalPointer());

    if (parentItem->hasChild(row))
        return createIndex(row, column, const_cast<iscore::Node*>(&parentItem->childAt(row)));
    else
        return QModelIndex();
}

QModelIndex MessageItemModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    auto childNode = static_cast<iscore::Node*>(index.internalPointer());
    auto parentNode = childNode->parent();

    if (!parentNode || parentNode == &m_rootNode)
        return QModelIndex();

    auto grandParentNode = parentNode->parent();
    if (!grandParentNode || grandParentNode == &m_rootNode)
        return QModelIndex();

    const int rowParent = grandParentNode->indexOfChild(parentNode);
    assert(rowParent != -1);
    return createIndex(rowParent, 0, parentNode);
}

int MessageItemModel::rowCount(const QModelIndex &parent) const
{
    if(parent.column() > 0)
        return 0;

    auto parentNode = nodeFromModelIndex(parent);

    if(!parentNode)
        return 0;

    return parentNode->childCount();
}

int MessageItemModel::columnCount(const QModelIndex &parent) const
{
    return (int)Column::Count;
}

bool MessageItemModel::hasChildren(const QModelIndex &parent) const
{
    auto parentNode = nodeFromModelIndex(parent);
    return parentNode->childCount() > 0;
}

QVariant MessageItemModel::data(const QModelIndex &index, int role) const
{
    const int col = index.column();

    if(col < 0 || col >= (int)Column::Count)
        return {};

    auto node = nodeFromModelIndex(index);

    if(! node)
        return {};

    switch((Column)col)
    {
        case Column::Name:
        {
            return nameColumnData(*node, role);
            break;
        }
        case Column::Value:
        {
            return valueColumnData(*node, role);
            break;
        }
        default:
            break;
    }

    return {};
}

bool MessageItemModel::setData(
        const QModelIndex &index,
        const QVariant &value,
        int role)
{
    return false;
}

QVariant MessageItemModel::headerData(
        int section,
        Qt::Orientation orientation,
        int role) const
{
    static const QMap<Column, QString> HEADERS{
        {Column::Name, QObject::tr("Address")},
        {Column::Value, QObject::tr("Value")}
    };
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if(section >= 0 && section < (int)Column::Count)
        {
            return HEADERS[(Column)section];
        }
    }

    return QVariant();
}

bool MessageItemModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
{
    return false;
}

QStringList MessageItemModel::mimeTypes() const
{
    return {iscore::mime::state()};
}

QMimeData *MessageItemModel::mimeData(const QModelIndexList &indexes) const
{
    return nullptr;
}

bool MessageItemModel::canDropMimeData(
        const QMimeData *data,
        Qt::DropAction action,
        int row,
        int column,
        const QModelIndex &parent) const
{
    if(action == Qt::IgnoreAction)
    {
        return true;
    }

    if(action != Qt::MoveAction && action != Qt::CopyAction)
    {
        return false;
    }

    // TODO extended to accept mime::device, mime::address, mime::node?
    if(! data || (! data->hasFormat(iscore::mime::messagelist())))
    {
        return false;
    }

    return true;
}

bool MessageItemModel::dropMimeData(
        const QMimeData *data,
        Qt::DropAction action,
        int row,
        int column,
        const QModelIndex &parent)
{
    if(action == Qt::IgnoreAction)
    {
        return true;
    }

    if(action != Qt::MoveAction && action != Qt::CopyAction)
    {
        return false;
    }

    // TODO extended to accept mime::device, mime::address, mime::node?
    if(! data || (! data->hasFormat(iscore::mime::messagelist())))
    {
        return false;
    }

    iscore::MessageList ml;
    fromJsonArray(
                QJsonDocument::fromJson(data->data(iscore::mime::messagelist())).array(),
                ml);

    auto cmd = new AddMessagesToModel{
               iscore::IDocument::path(*this),
               ml};

    CommandDispatcher<> disp(*m_stack);
    disp.submitCommand(cmd);

    return true;
}

Qt::DropActions MessageItemModel::supportedDropActions() const
{
    return Qt::IgnoreAction;
}

Qt::DropActions MessageItemModel::supportedDragActions() const
{
    return Qt::IgnoreAction;
}


Qt::ItemFlags MessageItemModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = Qt::ItemIsEnabled;
    if(index.isValid())
    {
        f |= Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
        if(index.column() == (int) Column::Name)
            f |= Qt::ItemIsDropEnabled;
    }
    else
    {
        f |= Qt::ItemIsDropEnabled;
    }
    return f;
}
