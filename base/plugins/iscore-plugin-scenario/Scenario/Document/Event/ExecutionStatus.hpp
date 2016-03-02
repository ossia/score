#pragma once
#include <Process/Style/ColorReference.hpp>
#include <QObject>

class QColor;

namespace Scenario
{
enum class ExecutionStatus { Waiting, Pending, Happened, Disposed, Editing };


// TODO Use me for events, states
class ExecutionStatusProperty final : public QObject
{
        Q_OBJECT
    public:
        ExecutionStatus get() const noexcept { return m_status; }
        void set(ExecutionStatus e) noexcept
        {
            if(m_status != e)
            {
                m_status = e;
                emit changed(m_status);
            }
        }
        ColorRef eventStatusColor();
        ColorRef stateStatusColor();

    signals:
        void changed(ExecutionStatus);

    private:
        ExecutionStatus m_status{ExecutionStatus::Editing};
};
}

Q_DECLARE_METATYPE(Scenario::ExecutionStatus)
