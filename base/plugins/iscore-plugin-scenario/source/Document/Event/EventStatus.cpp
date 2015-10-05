#include "EventStatus.hpp"
#include <ProcessInterface/Style/ScenarioStyle.hpp>

const QColor& eventStatusColor(EventStatus e)
{
    static const QColor error = Qt::magenta;
    const auto& col = ScenarioStyle::instance();
    switch(e)
    {
        case EventStatus::Editing:  return col.eventDefault;
        case EventStatus::Waiting:  return col.eventWaiting;
        case EventStatus::Pending:  return col.eventPending;
        case EventStatus::Happened: return col.eventHappened;
        case EventStatus::Disposed: return col.eventDisposed;
        default: return error;
    }
}
