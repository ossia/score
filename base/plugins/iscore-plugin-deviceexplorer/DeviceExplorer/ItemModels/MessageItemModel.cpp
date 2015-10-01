#include "MessageItemModel.hpp"
#include "NodeDisplayMethods.hpp"
#include "Commands/AddMessagesToModel.hpp"
#include <State/MessageListSerialization.hpp>
#include <iscore/document/DocumentInterface.hpp>

using namespace iscore;
MessageItemModel::MessageItemModel(
        CommandStack& stack,
        QObject* parent):
    TreeNodeBasedItemModel<MessageNode>{parent},
    m_rootNode{},
    m_stack{stack}
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

MessageItemModel &MessageItemModel::operator=(const node_type & n)
{
    beginResetModel();
    m_rootNode = n;
    endResetModel();
    return *this;
}

MessageItemModel &MessageItemModel::operator=(node_type && n)
{
    beginResetModel();
    m_rootNode = std::move(n);
    endResetModel();
    return *this;
}

iscore::Address address(const MessageItemModel::node_type &treeNode)
{
    iscore::Address addr;
    auto n = &treeNode;
    while(n->parent() && n->parent()->parent())
    {
        addr.path.prepend(n->name);
        n = n->parent();
    }

    ISCORE_ASSERT(n);
    addr.device = n->name;

    return addr;
}

iscore::Message message(const MessageItemModel::node_type& node)
{
    iscore::Message mess;
    mess.address = address(node);
    mess.value = node.value();

    return mess;
}

// TESTME
static void flatten_rec(iscore::MessageList& ml, const MessageItemModel::node_type& node)
{
    if(node.hasValue())
    {
        ml.append(message(node));
    }

    for(const auto& child : node)
    {
        flatten_rec(ml, child);
    }
}

MessageList MessageItemModel::flatten() const
{
    iscore::MessageList ml;
    flatten_rec(ml, m_rootNode);
    return ml;
}

// TODO another one to refactor with merges
void merge_impl(
        MessageItemModel::node_type& base,
        const StateNodeMessage& message)
{
    using iscore::Node;

    QStringList path = message.addr;

    ptr<MessageItemModel::node_type> node = &base;
    for(int i = 0; i < path.size(); i++)
    {
        auto& children = node->children();
        auto it = std::find_if(
                    children.begin(), children.end(),
                    [&] (const auto& cur_node) {
            return cur_node.displayName() == path[i];
        });

        if(it == children.end())
        {
            // We have to start adding sub-nodes from here.
            ptr<MessageItemModel::node_type> parentnode{node};
            for(int k = i; k < path.size(); k++)
            {
                ptr<MessageItemModel::node_type> newNode;
                if(k < path.size() - 1)
                {
                    newNode = &parentnode->emplace_back(
                                StateNodeData{path[k],
                                              OptionalValue{},
                                              OptionalValue{}},
                                nullptr);
                }
                else
                {
                    newNode = &parentnode->emplace_back(
                                StateNodeData{path[k],
                                              message.processValue,
                                              message.userValue},
                                nullptr);
                }

                parentnode = newNode;
            }

            break;
        }
        else
        {
            node = &*it;

            if(i == path.size() - 1)
            {
                // We replace the value by the one in the message
                node->processValue = message.processValue;
                node->userValue = message.userValue;
            }
        }
    }
}

// TESTME
void MessageItemModel::merge(const StateNodeMessage& mess)
{
    // First, try to see if there is a corresponding node

    auto n = try_getNodeFromString(m_rootNode, QStringList(mess.addr));
    if(n)
    {
        n->processValue = mess.processValue;
        n->userValue = mess.userValue;

        auto parent = n->parent();
        auto idx = createIndex(parent->indexOfChild(n), 0, n->parent());
        emit dataChanged(idx, idx);
    }
    else
    {
        // We need to create a node.
        merge_impl(m_rootNode, mess);
    }
}

int MessageItemModel::columnCount(const QModelIndex &parent) const
{
    return (int)Column::Count;
}

QVariant nameColumnData(const MessageItemModel::node_type& node, int role)
{
    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        return node.displayName();
    }

    return {};
}

QVariant valueColumnData(const MessageItemModel::node_type& node, int role)
{
    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        return node.displayValue();
    }

    return {};
}

QVariant MessageItemModel::data(const QModelIndex &index, int role) const
{
    const int col = index.column();

    if(col < 0 || col >= (int)Column::Count)
        return {};

    auto node = nodeFromModelIndex(index);

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

    auto cmd = new AddMessagesToModel{*this, ml};

    CommandDispatcher<> disp(m_stack);
    disp.submitCommand(cmd);

    return true;
}

Qt::DropActions MessageItemModel::supportedDropActions() const
{
    return Qt::IgnoreAction | Qt::MoveAction | Qt::CopyAction;
}

Qt::DropActions MessageItemModel::supportedDragActions() const
{
    return Qt::IgnoreAction;
}


Qt::ItemFlags MessageItemModel::flags(const QModelIndex &index) const
{
    ISCORE_TODO;
    Qt::ItemFlags f = Qt::ItemIsEnabled;

    if(index.isValid())
    {
        f |= Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;

        if(index.column() == (int)Column::Value)
            f |= Qt::ItemIsEditable;
    }
    else
    {
        f |= Qt::ItemIsDropEnabled;
    }
    return f;
}


#include "Plugin/Commands/EditValue.hpp"
bool MessageItemModel::setData(
        const QModelIndex& index,
        const QVariant& value,
        int role)
{
    if(! index.isValid())
        return false;

    auto n = nodeFromModelIndex(index);

    if(!n->parent() || !n->parent()->parent())
        return false;

    if(!n->hasValue())
        return false;

    auto col = Column(index.column());

    if(role == Qt::EditRole)
    {
        if(col == Column::Value)
        {
            // In this case we don't make a command, but we directly push the
            // new value.
            QVariant copy = value;
            auto res = copy.convert(n->value().val.type());
            if(res)
            {
                auto cmd = new EditValue(*this,
                                         MessageNodePath(*n),
                                         copy);

                CommandDispatcher<> disp(m_stack);
                disp.submitCommand(cmd);
                return true;
            }

            return false;
        }
    }

    return false;
}

void MessageItemModel::editData(
        const MessageNodePath& path,
        const OptionalValue& processValue,
        const OptionalValue& userValue)
{
    auto n = path.toNode(&rootNode());
    ISCORE_ASSERT(n && n->parent());

    QModelIndex index = createIndex(
                            n->parent()->indexOfChild(n),
                            (int)Column::Value,
                            n->parent());

    QModelIndex changedTopLeft = index;
    QModelIndex changedBottomRight = index;

    if(!n->parent()->parent())
        return;

    n->processValue = processValue;
    n->userValue = userValue;

    emit dataChanged(changedTopLeft, changedBottomRight);
}
