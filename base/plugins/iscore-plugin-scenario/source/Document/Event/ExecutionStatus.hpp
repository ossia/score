#pragma once
#include <QMetaType>
#include <QMap>
#include <QColor>
enum class EventStatus { Waiting, Pending, Happened, Disposed, Editing };
Q_DECLARE_METATYPE(EventStatus)


const QColor& eventStatusColor(EventStatus);
