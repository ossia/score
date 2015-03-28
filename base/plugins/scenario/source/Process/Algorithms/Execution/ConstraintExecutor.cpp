#include "ConstraintExecutor.hpp"

#include <ProcessInterface/ProcessExecutor.hpp>
#include <ProcessInterface/ProcessSharedModelInterface.hpp>
#include "Document/Constraint/ConstraintModel.hpp"
ConstraintExecutor::ConstraintExecutor(ConstraintModel& cm):
    m_constraint{cm}
{
    m_timer.setInterval(40);
    QObject::connect(&m_timer, &QTimer::timeout, [&] ( )
    {
        m_currentTime.addMSecs(m_timer.interval());
        if(m_currentTime < m_constraint.defaultDuration())
        {
            tick();
        }
        else
        {
            stop();
        }
    });

    for(auto& process : m_constraint.processes())
    {
        m_executors.push_back(process->makeExecutor());
    }
}

bool ConstraintExecutor::is(id_type<ConstraintModel> cm) const
{
    return cm == m_constraint.id();
}

bool ConstraintExecutor::evaluating() const
{
    return m_currentTime > m_constraint.minDuration();
}


void ConstraintExecutor::start()
{
    m_timer.start();
    for(auto& proc : m_executors)
    {
        proc->start();
    }
}


void ConstraintExecutor::stop()
{
    for(auto& proc : m_executors)
    {
        proc->stop();
    }
}


void ConstraintExecutor::tick()
{
    for(auto& proc : m_executors)
    {
        proc->onTick(m_currentTime);
    }
}
