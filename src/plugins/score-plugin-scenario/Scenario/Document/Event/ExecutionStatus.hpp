#pragma once
#include <score/model/ColorReference.hpp>

#include <verdigris>
class QColor;
namespace Process
{
struct Style;
}

namespace Scenario
{

// See ossia::time_event
enum class OffsetBehavior : int8_t
{
  True,
  False,
  Expression
};

enum class ExecutionStatus : int8_t
{
  Waiting,
  Pending,
  Happened,
  Disposed,
  Editing
};

// TODO Use me for events, states
struct ExecutionStatusProperty
{
  ExecutionStatus status{ExecutionStatus::Editing};

  ExecutionStatus get() const noexcept { return status; }
  void set(ExecutionStatus e) noexcept
  {
    if (status != e)
    {
      status = e;
    }
  }
  const score::Brush& eventStatusColor(const Process::Style&) const noexcept;
  const score::Brush& stateStatusColor(const Process::Style&) const noexcept;
  const score::Brush& conditionStatusColor(const Process::Style&) const noexcept;
};
}

Q_DECLARE_METATYPE(Scenario::ExecutionStatus)
Q_DECLARE_METATYPE(Scenario::OffsetBehavior)
W_REGISTER_ARGTYPE(Scenario::ExecutionStatus)
W_REGISTER_ARGTYPE(Scenario::OffsetBehavior)
