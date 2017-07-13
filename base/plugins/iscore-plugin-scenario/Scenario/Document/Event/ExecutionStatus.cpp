// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Style/ScenarioStyle.hpp>
#include <QColor>
#include <qnamespace.h>

#include "ExecutionStatus.hpp"

namespace Scenario
{
iscore::ColorRef ExecutionStatusProperty::eventStatusColor()
{
  const auto& col = ScenarioStyle::instance();
  switch (m_status)
  {
    case ExecutionStatus::Editing:
      return col.EventDefault;
    case ExecutionStatus::Waiting:
      return col.EventWaiting;
    case ExecutionStatus::Pending:
      return col.EventPending;
    case ExecutionStatus::Happened:
      return col.EventHappened;
    case ExecutionStatus::Disposed:
      return col.EventDisposed;
    default:
      return iscore::ColorRef(&iscore::Skin::instance().Warn3);
  }
}

iscore::ColorRef ExecutionStatusProperty::stateStatusColor()
{
  const auto& col = ScenarioStyle::instance();
  switch (m_status)
  {
    case ExecutionStatus::Editing:
      return col.StateOutline;
    case ExecutionStatus::Waiting:
      return col.EventWaiting;
    case ExecutionStatus::Pending:
      return col.EventPending;
    case ExecutionStatus::Happened:
      return col.EventHappened;
    case ExecutionStatus::Disposed:
      return col.EventDisposed;
    default:
      return iscore::ColorRef(&iscore::Skin::instance().Warn3);
  }
}
}
