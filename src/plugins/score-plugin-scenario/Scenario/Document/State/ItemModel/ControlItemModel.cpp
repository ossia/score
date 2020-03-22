#include "ControlItemModel.hpp"
#include <Process/ControlMessage.hpp>
#include <State/ValueConversion.hpp>
#include <ossia/network/value/value_traits.hpp>

#include <QMimeData>

#include <Scenario/Document/State/StateModel.hpp>
#include <score/document/DocumentContext.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

namespace Scenario
{
namespace
{
QVariant valueColumnData(const Process::ControlMessage& ctrl, int role)
{
  if (role == Qt::DisplayRole || role == Qt::EditRole)
  {
    const auto& val = ctrl.value;
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
  for(auto&& item : std::move(vec)) {
    auto it = ossia::find_if(cur, [&] (auto& ctl) { return ctl.port == item.port; });
    if (it == cur.end())
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
  if (index.row() < (int32_t)m_msgs.size())
  {
    switch (role)
    {
    case Qt::DisplayRole:
      switch(index.column())
      {
        case 0: return m_msgs[index.row()].name(m_state.context());
        case 1: return valueColumnData(m_msgs[index.row()], role);
      }
    }
  }

  return {};
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

  if (index.isValid())
  {
    f |= Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;

    if (index.column() == 1)
      f |= Qt::ItemIsEditable;
  }
  return f;
}

}
