#include "MessageItemModel.hpp"

#include <State/MessageListSerialization.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include <Scenario/Commands/State/UpdateState.hpp>
using namespace iscore;
MessageItemModel::MessageItemModel(
        CommandStack& stack,
        const StateModel& sm,
        QObject* parent):
    TreeNodeBasedItemModel<MessageNode>{parent},
    stateModel{sm},
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
        const auto& val = node.value();
        if(val)
            return iscore::convert::value<QVariant>(*val);
        return QVariant{};
    }

    return {};
}

QVariant MessageItemModel::data(const QModelIndex &index, int role) const
{
    const int col = index.column();

    if(col < 0 || col >= (int)Column::Count)
        return {};

    auto& node = nodeFromModelIndex(index);

    switch((Column)col)
    {
        case Column::Name:
        {
            return nameColumnData(node, role);
            break;
        }
        case Column::Value:
        {
            return valueColumnData(node, role);
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

    auto cmd = new AddMessagesToState{*this, ml};

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


bool MessageItemModel::setData(
        const QModelIndex& index,
        const QVariant& value_received,
        int role)
{
    if(! index.isValid())
        return false;

    auto& n = nodeFromModelIndex(index);

    if(!n.parent() || !n.parent()->parent())
        return false;

    if(!n.hasValue())
        return false;

    auto col = Column(index.column());

    if(role == Qt::EditRole)
    {
        if(col == Column::Value)
        {
            auto value = iscore::convert::toValue(value_received);
            auto current_val = n.value();
            if(current_val)
            {
                iscore::convert::convert(*current_val, value);
            }
            auto cmd = new AddMessagesToState{*this,
                                     iscore::MessageList{{address(n), value}}};

            CommandDispatcher<> disp(m_stack);
            disp.submitCommand(cmd);
            return true;
        }
    }

    return false;
}
