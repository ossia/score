#pragma once
#include <QObject>
#include <score/model/ColorReference.hpp>

class QColor;

namespace Scenario
{

// See ossia::time_event
enum class OffsetBehavior: int8_t
{
  True,
  False,
  Expression
};

enum class ExecutionStatus: int8_t
{
  Waiting,
  Pending,
  Happened,
  Disposed,
  Editing
};

// TODO Use me for events, states
class ExecutionStatusProperty
{
public:
  ExecutionStatus get() const noexcept
  {
    return m_status;
  }
  void set(ExecutionStatus e) noexcept
  {
    if (m_status != e)
    {
      m_status = e;
    }
  }
  score::ColorRef eventStatusColor();
  score::ColorRef stateStatusColor();


private:
  ExecutionStatus m_status{ExecutionStatus::Editing};
};
}

Q_DECLARE_METATYPE(Scenario::ExecutionStatus)
Q_DECLARE_METATYPE(Scenario::OffsetBehavior)
