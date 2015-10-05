#include "EventStatus.hpp"
#include <ProcessInterface/Style/ScenarioStyle.hpp>

const QColor& eventStatusColor(EventStatus e)
{
    static const QColor error = Qt::magenta;
    const auto& col = ScenarioStyle::instance();
    switch(e)
    {
        case EventStatus::Editing:  return col.EventDefault;
        case EventStatus::Waiting:  return col.EventWaiting;
        case EventStatus::Pending:  return col.EventPending;
        case EventStatus::Happened: return col.EventHappened;
        case EventStatus::Disposed: return col.EventDisposed;
        default: return error;
    }
}
