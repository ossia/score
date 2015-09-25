#include "MessageItemModel.hpp"
#include "NodeDisplayMethods.hpp"
#include "Commands/AddMessagesToModel.hpp"
#include <State/MessageListSerialization.hpp>
#include <iscore/document/DocumentInterface.hpp>

using namespace iscore;
MessageItemModel::MessageItemModel(
        CommandStack& stack,
        QObject* parent):
    NodeBasedItemModel{parent},
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

// TESTME
static void flatten_rec(iscore::MessageList& ml, const iscore::Node& node)
{
    if(node.is<iscore::AddressSettings>())
    {
        const auto& set = node.get<iscore::AddressSettings>();
        if(hasOutput(set.ioType))
        {
            ml.append(iscore::message(node));
        }
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

void MessageItemModel::insert(const Message& mess)
{
    // First, try to see if there is a corresponding node
    auto n = try_getNodeFromAddress(m_rootNode, mess.address);
    qDebug() << Q_FUNC_INFO << mess.address.toString() << mess.value.val;
    if(n)
    {
        qDebug() << "item model: setting" << mess.address.toString() << "at" << mess.value.toString();
        auto val = mess.value.val;
        if(!val.canConvert(n->get<iscore::AddressSettings>().value.val.type()))
            return;
        n->get<iscore::AddressSettings>().value.val = val;

        auto parent = n->parent();
        auto idx = createIndex(parent->indexOfChild(n), 0, n->parent());
        emit dataChanged(idx, idx);
    }
    else
    {
        ISCORE_TODO;
        // We need to create a node.
    }
}

int MessageItemModel::columnCount(const QModelIndex &parent) const
{
    return (int)Column::Count;
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

    auto cmd = new AddMessagesToModel{
               iscore::IDocument::path(*this),
               ml};

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

        Node* n = nodeFromModelIndex(index);
        if(n->isEditable())
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

    if(!n->is<AddressSettings>())
        return false;

    const auto& addr = n->get<iscore::AddressSettings>();
    if(!hasOutput(addr.ioType))
        return false;

    auto col = Column(index.column());

    if(role == Qt::EditRole)
    {
        if(col == Column::Value)
        {
            // In this case we don't make a command, but we directly push the
            // new value.
            QVariant copy = value;
            auto res = copy.convert(addr.value.val.type());
            if(res)
            {
                auto cmd = new EditValue(iscore::IDocument::path(*this),
                                         iscore::NodePath(*n),
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
        const NodePath& path,
        const QVariant& val)
{
    Node* node = path.toNode(&rootNode());
    ISCORE_ASSERT(node && node->parent());

    QModelIndex index = createIndex(
                            node->parent()->indexOfChild(node),
                            (int)Column::Value,
                            node->parent());

    QModelIndex changedTopLeft = index;
    QModelIndex changedBottomRight = index;

    if(!node->is<AddressSettings>())
        return;

    node->get<iscore::AddressSettings>().value.val = val;

    emit dataChanged(changedTopLeft, changedBottomRight);
}
