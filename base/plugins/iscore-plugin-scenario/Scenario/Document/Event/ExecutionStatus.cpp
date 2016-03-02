#include <Process/Style/ScenarioStyle.hpp>
#include <QColor>
#include <qnamespace.h>

#include "ExecutionStatus.hpp"

namespace Scenario
{
ColorRef ExecutionStatusProperty::eventStatusColor()
{
    const auto& col = ScenarioStyle::instance();
    switch(m_status)
    {
        case ExecutionStatus::Editing:  return col.EventDefault;
        case ExecutionStatus::Waiting:  return col.EventWaiting;
        case ExecutionStatus::Pending:  return col.EventPending;
        case ExecutionStatus::Happened: return col.EventHappened;
        case ExecutionStatus::Disposed: return col.EventDisposed;
        default: return ColorRef(&Skin::instance().Warn3);
    }
}

ColorRef ExecutionStatusProperty::stateStatusColor()
{
    const auto& col = ScenarioStyle::instance();
    switch(m_status)
    {
        case ExecutionStatus::Editing:  return col.StateOutline;
        case ExecutionStatus::Waiting:  return col.EventWaiting;
        case ExecutionStatus::Pending:  return col.EventPending;
        case ExecutionStatus::Happened: return col.EventHappened;
        case ExecutionStatus::Disposed: return col.EventDisposed;
        default: return ColorRef(&Skin::instance().Warn3);
    }
}
}
