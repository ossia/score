#include "ConstraintExecutor.hpp"

#include <ProcessInterface/ProcessExecutor.hpp>
#include <ProcessInterface/ProcessSharedModelInterface.hpp>
#include "Document/Constraint/ConstraintModel.hpp"
#include "ScenarioExecutor.hpp"
#include "Process/ScenarioModel.hpp"
#include "../curve_plugin/Automation/AutomationExecutor.hpp"
#include "../curve_plugin/Automation/AutomationModel.hpp"
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

    for(ProcessSharedModelInterface* process : m_constraint.processes())
    {
        if(process->processName() == "Scenario")
            m_executors.push_back(new ScenarioExecutor(static_cast<ScenarioModel&>(*process)));
        if(process->processName() == "Automation")
            m_executors.push_back(new AutomationExecutor(static_cast<AutomationModel&>(*process)));
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
    m_constraint.setPlayDuration(m_currentTime);
/*
    if((m_constraint.defaultDuration() != m_constraint.minDuration()
     || m_constraint.defaultDuration() != m_constraint.maxDuration())
    && m_currentTime > m_constraint.minDuration())
    {
        m_constraint.setDefaultDurationInBounds(m_currentTime);
    }
    */
}
