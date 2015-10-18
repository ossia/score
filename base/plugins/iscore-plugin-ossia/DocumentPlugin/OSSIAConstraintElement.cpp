#include "OSSIAConstraintElement.hpp"
#include <API/Headers/Editor/TimeConstraint.h>
#include <API/Headers/Editor/TimeProcess.h>
#include "../iscore-plugin-scenario/source/Document/Constraint/ConstraintModel.hpp"
#include "OSSIAScenarioElement.hpp"

#include <boost/range/algorithm.hpp>
#include "../iscore-plugin-curve/Automation/AutomationModel.hpp"
#include "OSSIAAutomationElement.hpp"
#include "OSSIAScenarioElement.hpp"
#include "iscore2OSSIA.hpp"
#include "OSSIA2iscore.hpp"

#include <sstream>

OSSIAConstraintElement::OSSIAConstraintElement(
        std::shared_ptr<OSSIA::TimeConstraint> ossia_cst,
        ConstraintModel& iscore_cst,
        QObject* parent):
    QObject{parent},
    m_iscore_constraint{iscore_cst},
    m_ossia_constraint{ossia_cst}
{
    con(iscore_cst.processes, &NotifyingMap<Process>::added,
            this, &OSSIAConstraintElement::on_processAdded);
    con(iscore_cst.processes, &NotifyingMap<Process>::removed,
            this, &OSSIAConstraintElement::on_processRemoved);

    con(iscore_cst, &ConstraintModel::loopingChanged,
        this, &OSSIAConstraintElement::on_loopingChanged);

    // Setup updates
    con(iscore_cst.duration, &ConstraintDurations::defaultDurationChanged, this,
            [=] (const TimeValue& t) {
        ossia_cst->setDuration(iscore::convert::time(t));
    });
    con(iscore_cst.duration, &ConstraintDurations::minDurationChanged, this,
            [=] (const TimeValue& t) {
        try {
            ossia_cst->setDurationMin(iscore::convert::time(t));
        }
        catch(std::runtime_error& e)
        {
            qWarning() << e.what();
        }
    });
    con(iscore_cst.duration, &ConstraintDurations::maxDurationChanged, this,
            [=] (const TimeValue& t) {
        try {
            ossia_cst->setDurationMax(iscore::convert::time(t));
        }
        catch(std::runtime_error& e)
        {
            qWarning() << e.what();
        }
    });

    if(iscore_cst.objectName() != QString("BaseConstraintModel"))
    {
        ossia_cst->setCallback([&] (
                               const OSSIA::TimeValue& position,
                               const OSSIA::TimeValue& date,
                               const std::shared_ptr<OSSIA::StateElement>& state)
        {
            constraintCallback(position, date, state);
        });
    }

    for(const auto& process : iscore_cst.processes)
    {
        on_processAdded(process);
    }
}

std::shared_ptr<OSSIA::TimeConstraint> OSSIAConstraintElement::OSSIAConstraint() const
{
    return m_ossia_constraint;
}

ConstraintModel&OSSIAConstraintElement::iscoreConstraint() const
{
    return m_iscore_constraint;
}

void OSSIAConstraintElement::play()
{
    m_iscore_constraint.duration.setPlayPercentage(0);
    m_ossia_constraint->setOffset(0.);
    m_ossia_constraint->start();
    executionStarted();
}

void OSSIAConstraintElement::stop()
{
    m_ossia_constraint->stop();
    for(auto& process : m_processes)
    {
        process.second.element->stop();
    }

    m_iscore_constraint.reset();
    executionStopped();
}

void OSSIAConstraintElement::executionStarted()
{
    m_iscore_constraint.duration.setPlayPercentage(0);
    for(Process& proc : m_iscore_constraint.processes)
    {
        proc.startExecution();
    }
}

void OSSIAConstraintElement::executionStopped()
{
    for(Process& proc : m_iscore_constraint.processes)
    {
        proc.stopExecution();
    }
}

void OSSIAConstraintElement::on_processAdded(
        const Process& iscore_proc) // TODO ProcessExecutionView
{
    // The DocumentPlugin creates the elements in the processes.
    // TODO maybe have an execution_view template on processes, that
    // gives correct const / non_const access ?
    auto proc = const_cast<Process*>(&iscore_proc);
    OSSIAProcessElement* plug{};
    if(auto scenar = dynamic_cast<ScenarioModel*>(proc))
    {
        plug = new OSSIAScenarioElement{*this, *scenar, proc};
    }
    else if(auto autom = dynamic_cast<AutomationModel*>(proc))
    {
        plug = new OSSIAAutomationElement{*this, *autom, proc};
    }

    if(plug)
    {
        auto id = iscore_proc.id();
        m_processes.insert(std::make_pair(id,
                            OSSIAProcess(plug,
                             std::make_unique<ProcessWrapper>(
                                 m_ossia_constraint,
                                 plug->process(),
                                 iscore::convert::time(plug->iscoreProcess().duration()),
                                 m_iscore_constraint.looping()
                             )
                            )
                           ));

        // i-score scenario has ownership, hence
        // we have to remove it from the array if deleted
        connect(plug, &QObject::destroyed, this,
                [=] (QObject*) {
            // The OSSIA::Process removal is in each process dtor
            m_processes.erase(id);
        }, Qt::DirectConnection);

        // Processes might change (for instance automation needs to be recreated
        // at each address change) so we do this little dance.
        connect(plug, &OSSIAProcessElement::changed,
                this, [=] (
                    const std::shared_ptr<OSSIA::TimeProcess>& a1,
                    const std::shared_ptr<OSSIA::TimeProcess>& a2) {
            m_processes.at(id).wrapper->changed(a1, a2);
        });

        connect(&plug->iscoreProcess(), &Process::durationChanged,
                this, [=] () {
            auto& proc = m_processes.at(id);
            proc.wrapper->setDuration(iscore::convert::time(proc.element->iscoreProcess().duration()));
        });
    }
}

void OSSIAConstraintElement::on_processRemoved(const Process& process)
{
    auto it = m_processes.find(process.id());
    if(it != m_processes.end())
    {
        // We don't have ownership so we don't delete. The ProcessModel has it.
        m_processes.erase(it);
    }
}

void OSSIAConstraintElement::on_loopingChanged(bool b)
{
    // Note : by default the processes are in a "basic" impl.
    // If the process starts looping, they are moved to the loop impl.

    for(auto& proc_pair : m_processes)
    {
        proc_pair.second.wrapper->setLooping(b);
    }
}

void OSSIAConstraintElement::constraintCallback(
        const OSSIA::TimeValue& position,
        const OSSIA::TimeValue& date,
        const std::shared_ptr<OSSIA::StateElement>& state)
{
    auto currentTime = OSSIA::convert::time(date);

    auto& cstdur = m_iscore_constraint.duration;
    const auto& maxdur = cstdur.maxDuration();

    if(!maxdur.isInfinite())
        cstdur.setPlayPercentage(currentTime / cstdur.maxDuration());
    else
        cstdur.setPlayPercentage(currentTime / cstdur.defaultDuration());
}
