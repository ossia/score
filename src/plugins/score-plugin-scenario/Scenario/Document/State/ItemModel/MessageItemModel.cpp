// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MessageItemModel.hpp"

#include <Process/State/MessageNode.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <State/Message.hpp>
#include <State/MessageListSerialization.hpp>
#include <State/ValueConversion.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeItemModel.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/network/value/value_traits.hpp>
#include <ossia-qt/name_utils.hpp>

#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QObject>
#include <QString>
#include <QUrl>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Scenario::MessageItemModel)
namespace Scenario
{
class StateModel;
MessageItemModel::MessageItemModel(const StateModel& sm, QObject* parent)
    : TreeNodeBasedItemModel<Process::MessageNode>{parent}, stateModel{sm}, m_rootNode{}
{
  this->setObjectName("Scenario::MessageItemModel");
}

MessageItemModel& MessageItemModel::operator=(const MessageItemModel& other)
{
  beginResetModel();
  m_rootNode = other.m_rootNode;
  endResetModel();
  return *this;
}

MessageItemModel& MessageItemModel::operator=(const node_type& n)
{
  beginResetModel();
  m_rootNode = n;
  endResetModel();
  return *this;
}

MessageItemModel& MessageItemModel::operator=(node_type&& n)
{
  beginResetModel();
  m_rootNode = std::move(n);
  endResetModel();
  return *this;
}

int MessageItemModel::columnCount(const QModelIndex& parent) const
{
  return (int)Column::Count;
}

static QVariant nameColumnData(const MessageItemModel::node_type& node, int role)
{
  if (role == Qt::DisplayRole || role == Qt::EditRole)
  {
    return node.displayName();
  }

  return {};
}

QVariant valueColumnData(const MessageItemModel::node_type& node, int role)
{
  if (role == Qt::DisplayRole || role == Qt::EditRole)
  {
    const auto& opt_val = node.value();
    if (opt_val)
    {
      auto& val = *opt_val;
      if (ossia::is_array(val))
      {
        // TODO a nice editor for lists.
        // TODO use AddressItemModel's !
        return State::convert::toPrettyString(val);
      }
      else
      {
        return State::convert::value<QVariant>(val);
      }
    }
    return QVariant{};
  }

  return {};
}

QVariant MessageItemModel::data(const QModelIndex& index, int role) const
{
  const int col = index.column();

  if (col < 0 || col >= (int)Column::Count)
    return {};

  auto& node = nodeFromModelIndex(index);

  switch ((Column)col)
  {
    case Column::Name:
    {
      return nameColumnData(node, role);
    }
    case Column::Value:
    {
      return valueColumnData(node, role);
    }
    default:
      break;
  }

  return {};
}

QVariant MessageItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
  {
    if (section == 0)
      return tr("Address");
    else if (section == 1)
      return tr("Value");
  }

  return QAbstractItemModel::headerData(section, orientation, role);
}

bool MessageItemModel::setHeaderData(
    int section,
    Qt::Orientation orientation,
    const QVariant& value,
    int role)
{
  return false;
}

QStringList MessageItemModel::mimeTypes() const
{
  return {score::mime::messagelist()};
}

struct SelectedNodes
{
  /**
   * @brief parents The topmost parents of the selected parameters
   */
  std::vector<Process::MessageNode*> parents;

  /**
   * @brief messages The selected messages
   */
  std::vector<Process::MessageNode*> messages;
};

QMimeData* MessageItemModel::mimeData(const QModelIndexList& indexes) const
{
  SelectedNodes nodes;
  ossia::transform(indexes, std::back_inserter(nodes.parents), [&](const QModelIndex& idx) {
    return &nodeFromModelIndex(idx);
  });
  nodes.parents = filterUniqueParents(nodes.parents);

  State::MessageList messages;
  for (auto* node : nodes.parents)
  {
    messages += flatten(*node);
  }

  if (!messages.empty())
  {
    auto mimeData = new QMimeData;
    Mime<State::MessageList>::Serializer s{*mimeData};
    s.serialize(messages);
    return mimeData;
  }

  return nullptr;
}

bool MessageItemModel::canDropMimeData(
    const QMimeData* data,
    Qt::DropAction action,
    int row,
    int column,
    const QModelIndex& parent) const
{
  if (action == Qt::IgnoreAction)
  {
    return true;
  }

  if (action != Qt::MoveAction && action != Qt::CopyAction)
  {
    return false;
  }

  if (!(data->hasFormat(score::mime::messagelist()) || data->hasUrls()))
  {
    return false;
  }

  return true;
}

bool MessageItemModel::dropMimeData(
    const QMimeData* data,
    Qt::DropAction action,
    int row,
    int column,
    const QModelIndex& parent)
{
  if (action == Qt::IgnoreAction)
  {
    return true;
  }

  if (action != Qt::MoveAction && action != Qt::CopyAction)
  {
    return false;
  }

  if (data->hasFormat(score::mime::messagelist()))
  {
    State::MessageList ml = MimeWriter<State::MessageList>{*data}.deserialize();

    auto cmd = new Command::AddMessagesToState{stateModel, std::move(ml)};

    CommandDispatcher<> disp(score::IDocument::documentContext(stateModel).commandStack);
    beginResetModel();
    disp.submit(cmd);
    endResetModel();
  }
  else if (data->hasUrls())
  {
    State::MessageList ml;
    for (const auto& u : data->urls())
    {
      auto path = u.toLocalFile();
      if (QFile f{path}; QFileInfo{f}.suffix() == "cues" && f.open(QIODevice::ReadOnly))
      {
        ml += fromJson<State::MessageList>(f.readAll());
      }
    }

    if (!ml.empty())
    {
      auto cmd = new Command::AddMessagesToState{stateModel, ml};
      CommandDispatcher<>{score::IDocument::documentContext(stateModel).commandStack}.submit(cmd);
    }
  }
  return false;
}

Qt::DropActions MessageItemModel::supportedDropActions() const
{
  return Qt::IgnoreAction | Qt::MoveAction | Qt::CopyAction;
}

Qt::DropActions MessageItemModel::supportedDragActions() const
{
  return Qt::CopyAction;
}

Qt::ItemFlags MessageItemModel::flags(const QModelIndex& index) const
{
  Qt::ItemFlags f = Qt::ItemIsEnabled;

  if (index.isValid())
  {
    f |= Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable;
  }
  else
  {
    f |= Qt::ItemIsDropEnabled;
  }
  return f;
}

bool MessageItemModel::setData(const QModelIndex& index, const QVariant& value_received, int role)
{
  if (!index.isValid())
    return false;

  auto& n = nodeFromModelIndex(index);

  if (!n.parent())
    return false;

  auto col = Column(index.column());

  if (role == Qt::EditRole)
  {
    switch(col)
    {
      case Column::Name:
      {
        QString name = value_received.toString();
        ossia::net::sanitize_name(name);
        if(name.isEmpty())
          return false;

        // TODO check that name is unique across brethren
        // TODO accessor
        auto cmd = new Scenario::Command::RenameAddressInState{stateModel, address(n), {name, {}}};

        CommandDispatcher<> disp(score::IDocument::documentContext(stateModel).commandStack);
        beginResetModel();
        disp.submit(cmd);
        endResetModel();
        return true;
      }

      case Column::Value:
      {
        if (!n.hasValue())
          return false;
        auto value = State::convert::fromQVariant(value_received);
        auto current_val = n.value();
        if (current_val)
        {
          State::convert::convert(*current_val, value);
        }
        auto cmd
            = new Command::AddMessagesToState{stateModel, State::MessageList{{address(n), value}}};

        CommandDispatcher<> disp(score::IDocument::documentContext(stateModel).commandStack);
        beginResetModel();
        disp.submit(cmd);
        endResetModel();
        return true;
      }
      default:
        break;
    }
  }

  return false;
}
}
