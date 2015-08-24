#include "StateItemModel.hpp"
using namespace iscore;
StateItemModel::StateItemModel():
    QAbstractItemModel{},
    m_rootNode(QString{"i am root"})
{

}

StateItemModel::StateItemModel(const StateItemModel & other):
    QAbstractItemModel{},
    m_rootNode{other.m_rootNode}
{

}

StateItemModel &StateItemModel::operator=(const StateItemModel & other)
{
    beginResetModel();
    m_rootNode = other.m_rootNode;
    endResetModel();
    return *this;
}

StateItemModel &StateItemModel::operator=(const StateNode & n)
{
    beginResetModel();
    m_rootNode = n;
    endResetModel();
    return *this;
}

StateItemModel &StateItemModel::operator=(StateNode && n)
{
    beginResetModel();
    m_rootNode = std::move(n);
    endResetModel();
    return *this;
}

StateNode *StateItemModel::nodeFromModelIndex(const QModelIndex &index)
{
    return index.isValid()
            ? static_cast<StateNode*>(index.internalPointer())
            : &m_rootNode;
}

const StateNode *StateItemModel::nodeFromModelIndex(const QModelIndex &index) const
{
    return index.isValid()
            ? static_cast<const StateNode*>(index.internalPointer())
            : &m_rootNode;
}

void StateItemModel::addState(StateNode *parent, StateNode *node, int row)
{
    Q_ASSERT(parent);
    Q_ASSERT(node);

    if (row == -1)
    {
        row = parent->childCount(); //insert as last child
    }

    int rowParent = 0;
    if(parent != &m_rootNode)
    {
        auto grandparent = parent->parent();
        Q_ASSERT(grandparent);

        rowParent = grandparent->indexOfChild(parent);
    }

    QModelIndex parentIndex = createIndex(rowParent, 0, parent);

    beginInsertRows(parentIndex, row, row);
    parent->insertChild(row, node);
    endInsertRows();
}

void StateItemModel::removeState(StateNode *node)
{
    Q_ASSERT(node);

    auto parent = node->parent();
    Q_ASSERT(parent);

    int rowParent = 0;
    if(parent != &m_rootNode)
    {
        auto grandparent = parent->parent();
        Q_ASSERT(grandparent);
        rowParent = grandparent->indexOfChild(parent);
    }

    QModelIndex parentIndex = createIndex(rowParent, 0, parent);

    int row = parent->indexOfChild(node);

    beginRemoveRows(parentIndex, row, row);
    parent->removeChild(node);
    endRemoveRows();

    delete node;
}

QModelIndex StateItemModel::index(int row,
                                  int column,
                                  const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    const StateNode* parentItem{};

    if (!parent.isValid())
        parentItem = &m_rootNode; // todo why ?
    else
        parentItem = static_cast<StateNode*>(parent.internalPointer());

    StateNode* childItem = parentItem->childAt(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex StateItemModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    auto childNode = static_cast<StateNode*>(index.internalPointer());
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

int StateItemModel::rowCount(const QModelIndex &parent) const
{
    if(parent.column() > 0)
        return 0;

    auto parentNode = nodeFromModelIndex(parent);

    if(!parentNode)
        return 0;

    return parentNode->childCount();
}

int StateItemModel::columnCount(const QModelIndex &parent) const
{
    return (int)Column::Count;
}

bool StateItemModel::hasChildren(const QModelIndex &parent) const
{
    auto parentNode = nodeFromModelIndex(parent);
    return parentNode->childCount() > 0;
}

QVariant StateItemModel::data(const QModelIndex &index, int role) const
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
            if(role == Qt::DisplayRole)
            {
                return node->name();
            }
            break;
        case Column::Messages:
        {
            if(role == Qt::DisplayRole)
            {
                if(node->is<iscore::MessageList>())
                {
                    const auto& ml = node->get<iscore::MessageList>();
                    QStringList str;
                    for(auto& mess : ml)
                    {
                        str += mess.toString();
                    }
                    return str.join("\n");
                }
            }
            break;
        }
        default:
            break;
    }

    return {};
}

bool StateItemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    return false;
}

QVariant StateItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    static const QMap<Column, QString> HEADERS{
        {Column::Name, QObject::tr("State")},
        {Column::Messages, QObject::tr("Messages")}
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

bool StateItemModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
{
    return false;
}

QStringList StateItemModel::mimeTypes() const
{
    return {iscore::mime::state()};
}

QMimeData *StateItemModel::mimeData(const QModelIndexList &indexes) const
{
    return nullptr;
}

bool StateItemModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const
{
    return false;
}

bool StateItemModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    return false;
}

Qt::DropActions StateItemModel::supportedDropActions() const
{
    return Qt::IgnoreAction;
}

Qt::DropActions StateItemModel::supportedDragActions() const
{
    return Qt::IgnoreAction;
}


Qt::ItemFlags StateItemModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = Qt::ItemIsEnabled;
    if(index.isValid())
    {
        auto n = nodeFromModelIndex(index);
        Q_ASSERT(n);

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
