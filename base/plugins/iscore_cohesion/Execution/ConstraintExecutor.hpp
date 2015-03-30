#pragma once
#include <QTimer>
#include <ProcessInterface/TimeValue.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
class ConstraintModel;
class ProcessExecutor;

class ConstraintExecutor
{
        QList<ProcessExecutor*> m_executors;
        ConstraintModel& m_constraint;
        QTimer m_timer;
        TimeValue m_currentTime;

        bool m_disabled{};
    public:
        ConstraintExecutor(ConstraintModel& cm);
        bool is(id_type<ConstraintModel> cm) const;
        bool evaluating() const;

        void start();
        void stop();
        void tick();

        void disable()
        { m_disabled = true; }
        bool disabled() const
        { return m_disabled; }
};
