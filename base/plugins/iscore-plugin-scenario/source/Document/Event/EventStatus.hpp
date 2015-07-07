#pragma once
#include <QMetaType>
enum class EventStatus { Waiting, Pending, Happened, Disposed, Editing };
Q_DECLARE_METATYPE(EventStatus)
