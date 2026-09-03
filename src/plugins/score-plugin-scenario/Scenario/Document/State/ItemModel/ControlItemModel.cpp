#include "ControlItemModel.hpp"

#include <State/ValueConversion.hpp>

#include <Process/ControlMessage.hpp>

#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/ItemModel/ValueItemDelegate.hpp>
#include <Scenario/Document/State/StateModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <ossia/detail/ssize.hpp>
#include <ossia/network/value/value_traits.hpp>

#include <QMimeData>

namespace Scenario
{
namespace
{
QVariant valueColumnData(const Process::ControlMessage& ctrl, int role)
{
  const auto& val = ctrl.value;
  if(role == Qt::DisplayRole || role == Qt::EditRole)
  {
    if(ossia::is_array(val))
    {
      return State::convert::toPrettyString(val);
    }
    else if(role == Qt::DisplayRole && val.get_type() == ossia::val_type::STRING)
    {
      return State::convert::stringCellText(
          QByteArray::fromStdString(*val.target<std::string>()));
    }
    else
    {
      return State::convert::value<QVariant>(val);
    }
  }
  else if(role == Qt::ToolTipRole)
  {
    if(const auto* s = val.target<std::string>())
    {
      const auto tip = State::convert::stringCellToolTip(QByteArray::fromStdString(*s));
      if(!tip.isEmpty())
        return tip;
    }
  }
  else if(role == OssiaValueRole)
  {
    return QVariant::fromValue(val);
  }

  return {};
}
}

ControlItemModel::ControlItemModel(Scenario::StateModel& ctx, QObject* parent)
    : QAbstractItemModel{parent}
    , m_state{ctx}
{
}

ControlItemModel::~ControlItemModel()
{
  if(!m_msgs.empty())
  {
    auto& sm = m_state.context().model<ScenarioDocumentModel>();
    auto it = ossia::find(sm.statesWithControls, &m_state);
    if(it != sm.statesWithControls.end())
      sm.statesWithControls.erase(it);
  }
}

void ControlItemModel::replaceWith(const std::vector<Process::ControlMessage>& c)
{
  bool wasEmpty = m_msgs.empty();
  bool isEmpty = c.empty();
  beginResetModel();
  m_msgs = c;
  endResetModel();

  m_state.sig_controlMessagesUpdated();

  if(wasEmpty && !isEmpty)
  {
    auto& sm = m_state.context().model<ScenarioDocumentModel>();
    sm.statesWithControls.push_back(&m_state);
  }
  else if(!wasEmpty && isEmpty)
  {
    auto& sm = m_state.context().model<ScenarioDocumentModel>();
    auto it = ossia::find(sm.statesWithControls, &m_state);
    if(it != sm.statesWithControls.end())
      sm.statesWithControls.erase(it);
  }
}

void ControlItemModel::addMessages(
    std::vector<Process::ControlMessage>& cur,
    std::vector<Process::ControlMessage>&& vec)
{
  for(auto&& item : std::move(vec))
  {
    auto it = ossia::find_if(cur, [&](auto& ctl) { return ctl.port == item.port; });
    if(it == cur.end())
    {
      cur.push_back(std::move(item));
    }
    else
    {
      it->value = std::move(item.value);
    }
  }
}

QModelIndex ControlItemModel::index(int row, int column, const QModelIndex& parent) const
{
  return createIndex(row, column, nullptr);
}

QModelIndex ControlItemModel::parent(const QModelIndex& child) const
{
  return {};
}

int ControlItemModel::rowCount(const QModelIndex& parent) const
{
  return m_msgs.size();
}

int ControlItemModel::columnCount(const QModelIndex& parent) const
{
  return 2;
}

QVariant ControlItemModel::data(const QModelIndex& index, int role) const
{
  if(index.row() < 0 || index.row() >= std::ssize(m_msgs))
    return {};

  if(index.column() == 0)
    return role == Qt::DisplayRole ? m_msgs[index.row()].name(m_state.context())
                                   : QVariant{};

  if(index.column() == 1)
    return valueColumnData(m_msgs[index.row()], role);

  return {};
}

bool ControlItemModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if(role != Qt::EditRole || index.column() != 1)
    return false;
  if(index.row() < 0 || index.row() >= std::ssize(m_msgs))
    return false;

  const auto& cur = m_msgs[index.row()];

  auto next = value.canConvert<ossia::value>() ? value.value<ossia::value>()
                                               : State::convert::fromQVariant(value);
  if(!next.valid())
    return false;

  if(cur.value.valid() && next.get_type() != cur.value.get_type()
     && !State::convert::convert(cur.value, next))
    return false;

  if(next == cur.value)
    return false;

  CommandDispatcher<>{m_state.context().commandStack}.submit(
      new Command::AddControlMessagesToState{
          m_state, std::vector<Process::ControlMessage>{{cur.port, next}}});
  return true;
}

Qt::DropActions ControlItemModel::supportedDragActions() const
{
  return Qt::CopyAction;
}

Qt::DropActions ControlItemModel::supportedDropActions() const
{
  return {};
}

Qt::ItemFlags ControlItemModel::flags(const QModelIndex& index) const
{
  Qt::ItemFlags f = Qt::ItemIsEnabled;

  if(index.isValid())
  {
    f |= Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;

    if(index.column() == 1)
      f |= Qt::ItemIsEditable;
  }
  return f;
}

}
