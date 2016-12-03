#pragma once
#include <QObject>
#include <iscore/model/ColorReference.hpp>

class QColor;

namespace Scenario
{

// See ossia::time_event
enum class OffsetBehavior
{
  True,
  False,
  Expression
};

enum class ExecutionStatus
{
  Waiting,
  Pending,
  Happened,
  Disposed,
  Editing
};

// TODO Use me for events, states
class ExecutionStatusProperty final : public QObject
{
  Q_OBJECT
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
      emit changed(m_status);
    }
  }
  iscore::ColorRef eventStatusColor();
  iscore::ColorRef stateStatusColor();

signals:
  void changed(ExecutionStatus);

private:
  ExecutionStatus m_status{ExecutionStatus::Editing};
};
}

Q_DECLARE_METATYPE(Scenario::ExecutionStatus)
Q_DECLARE_METATYPE(Scenario::OffsetBehavior)
