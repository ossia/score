#pragma once

#include <QState>
#include <QStateMachine>
inline bool isStateActive(QState* s)
{
#if (QT_VERSION < QT_VERSION_CHECK(5, 4, 0))
    return s->machine()->configuration().contains(s);
#else
    return s->active();
#endif
}

inline auto finishedState()
{
#if (QT_VERSION < QT_VERSION_CHECK(5, 5, 0))
    return finishedState();
#else
    return &QState::finished;
#endif
}
