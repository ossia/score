#pragma once
#include <QMetaType>
#include <QMap>
#include <QColor>
// TODO rename in ExecutionStatus
enum class EventStatus { Waiting, Pending, Happened, Disposed, Editing };
Q_DECLARE_METATYPE(EventStatus)


const QMap<EventStatus, QColor>& eventStatusColorMap();
