// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ExecutionStatus.hpp"

#include <Process/Style/ScenarioStyle.hpp>

#include <qnamespace.h>

namespace Scenario
{
const score::Brush& ExecutionStatusProperty::eventStatusColor(const Process::Style& col) const noexcept
{
  switch (status)
  {
    case ExecutionStatus::Editing:
      return col.EventDefault();
    case ExecutionStatus::Waiting:
      return col.EventWaiting();
    case ExecutionStatus::Pending:
      return col.EventPending();
    case ExecutionStatus::Happened:
      return col.EventDefault();
    case ExecutionStatus::Disposed:
      return col.EventDisposed();
    default:
      return col.skin.Warn3;
  }
}

const score::Brush& ExecutionStatusProperty::stateStatusColor(const Process::Style& col) const noexcept
{
  switch (status)
  {
    case ExecutionStatus::Editing:
      return col.StateOutline();
    case ExecutionStatus::Waiting:
      return col.EventWaiting();
    case ExecutionStatus::Pending:
      return col.EventPending();
    case ExecutionStatus::Happened:
      return col.StateDot();
    case ExecutionStatus::Disposed:
      return col.EventDisposed();
    default:
      return col.skin.Warn3;
  }
}

const score::Brush& ExecutionStatusProperty::conditionStatusColor(const Process::Style& col) const noexcept
{
  switch (status)
  {
    case ExecutionStatus::Editing:
      return col.ConditionDefault();
    case ExecutionStatus::Waiting:
      return col.EventWaiting();
    case ExecutionStatus::Pending:
      return col.EventPending();
    case ExecutionStatus::Happened:
      return col.EventHappened();
    case ExecutionStatus::Disposed:
      return col.EventDisposed();
    default:
      return col.skin.Warn3;
  }
}
}
