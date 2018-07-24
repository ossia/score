#pragma once

#include <QState>
#include <QStateMachine>
inline bool isStateActive(QState* s)
{
  return s->active();
}

inline constexpr auto finishedState()
{
  return &QState::finished;
}
