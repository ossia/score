#include <Process/Style/ScenarioStyle.hpp>
#include <qcolor.h>
#include <qnamespace.h>

#include "ExecutionStatus.hpp"

const QColor& eventStatusColor(ExecutionStatus e)
{
    static const QColor error = Qt::magenta;
    const auto& col = ScenarioStyle::instance();
    switch(e)
    {
        case ExecutionStatus::Editing:  return col.EventDefault;
        case ExecutionStatus::Waiting:  return col.EventWaiting;
        case ExecutionStatus::Pending:  return col.EventPending;
        case ExecutionStatus::Happened: return col.EventHappened;
        case ExecutionStatus::Disposed: return col.EventDisposed;
        default: return error;
    }
}
