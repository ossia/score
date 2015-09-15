#include "EventStatus.hpp"
#include <QMap>

namespace {
static const QMap<EventStatus, QColor> event_status_map{
    {EventStatus::Waiting, Qt::lightGray},
    {EventStatus::Pending, Qt::yellow},
    {EventStatus::Happened, QColor::fromRgb(34, 224, 0)},
    {EventStatus::Disposed, Qt::red},
    {EventStatus::Editing, Qt::white}
};
}

const QMap<EventStatus, QColor>& eventStatusColorMap()
{
    return event_status_map;
}
