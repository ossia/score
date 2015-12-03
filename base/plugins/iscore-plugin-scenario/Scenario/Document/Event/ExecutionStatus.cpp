#include <Process/Style/ScenarioStyle.hpp>
#include <QColor>
#include <qnamespace.h>

#include "ExecutionStatus.hpp"

const QColor& ExecutionStatusProperty::eventStatusColor()
{
    static const QColor error = Qt::magenta;
    const auto& col = ScenarioStyle::instance();
    switch(m_status)
    {
        case ExecutionStatus::Editing:  return col.EventDefault;
        case ExecutionStatus::Waiting:  return col.EventWaiting;
        case ExecutionStatus::Pending:  return col.EventPending;
        case ExecutionStatus::Happened: return col.EventHappened;
        case ExecutionStatus::Disposed: return col.EventDisposed;
        default: return error;
    }
}

const QColor& ExecutionStatusProperty::stateStatusColor()
{
    static const QColor error = Qt::magenta;
    const auto& col = ScenarioStyle::instance();
    switch(m_status)
    {
        case ExecutionStatus::Editing:  return col.Background;
        case ExecutionStatus::Waiting:  return col.EventWaiting;
        case ExecutionStatus::Pending:  return col.EventPending;
        case ExecutionStatus::Happened: return col.EventHappened;
        case ExecutionStatus::Disposed: return col.EventDisposed;
        default: return error;
    }
}
