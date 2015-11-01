#pragma once
#include <QMetaType>
#include <QMap>
#include <QColor>
#include <QObject>
enum class ExecutionStatus { Waiting, Pending, Happened, Disposed, Editing };
Q_DECLARE_METATYPE(ExecutionStatus)


const QColor& eventStatusColor(ExecutionStatus);

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

    signals:
        void changed(ExecutionStatus);

    private:
        ExecutionStatus m_status{ExecutionStatus::Editing};
};
