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

#include <sstream>

OSSIAConstraintElement::OSSIAConstraintElement(
        std::shared_ptr<OSSIA::TimeConstraint> ossia_cst,
        ConstraintModel& iscore_cst,
        QObject* parent):
    QObject{parent},
    m_iscore_constraint{iscore_cst},
    m_ossia_constraint{ossia_cst}
{
    con(iscore_cst, &ConstraintModel::processCreated,
            this, &OSSIAConstraintElement::on_processAdded);
    con(iscore_cst, &ConstraintModel::processRemoved,
            this, &OSSIAConstraintElement::on_processRemoved);

    // Setup updates
    // todo : should be in OSSIAConstraintElement
    con(iscore_cst.duration, &ConstraintDurations::defaultDurationChanged, this,
            [=] (const TimeValue& t) {
        ossia_cst->setDuration(iscore::convert::time(t));
    });
    con(iscore_cst.duration, &ConstraintDurations::minDurationChanged, this,
            [=] (const TimeValue& t) {
        ossia_cst->setDurationMin(iscore::convert::time(t));
    });
    con(iscore_cst.duration, &ConstraintDurations::maxDurationChanged, this,
            [=] (const TimeValue& t) {
        ossia_cst->setDurationMax(iscore::convert::time(t));
    });


    for(const auto& process : iscore_cst.processes())
    {
        on_processAdded(process);
    }
}

std::shared_ptr<OSSIA::TimeConstraint> OSSIAConstraintElement::constraint() const
{
    return m_ossia_constraint;
}

void OSSIAConstraintElement::stop()
{
    m_ossia_constraint->stop();
    for(auto& process : m_processes)
    {
        process.second->stop();
    }

    m_iscore_constraint.reset();
}

void OSSIAConstraintElement::on_processAdded(
        const Process& iscore_proc)
{
    // The DocumentPlugin creates the elements in the processes.
    auto proc = &iscore_proc;
    OSSIAProcessElement* plug{};
    if(auto scenar = dynamic_cast<const ScenarioModel*>(proc))
    {
        plug = new OSSIAScenarioElement{this, scenar, const_cast<Process*>(proc)};
    }
    else if(auto autom = dynamic_cast<const AutomationModel*>(proc))
    {
        plug = new OSSIAAutomationElement{this, autom, const_cast<Process*>(proc)};
    }

    if(plug)
    {
        m_processes.insert({iscore_proc.id(), plug});

        // i-score scenario has ownership, hence
        // we have to remove it from the array if deleted
        connect(plug, &QObject::destroyed, this,
                [&,id=iscore_proc.id()] (QObject*) {
            // The OSSIA::Process removal is in each process dtor
            m_processes.erase(id);
        }, Qt::DirectConnection);

        // Processes might change (for instance automation needs to be recreated
        // at each address change) so we do this little dance.
        connect(plug, &OSSIAProcessElement::changed,
                this, [=] (auto&& oldProc, auto&& newProc) {
            if(oldProc)
                m_ossia_constraint->removeTimeProcess(oldProc);

            if(newProc)
                m_ossia_constraint->addTimeProcess(plug->process());
        });


        if(plug->process())
        {
            m_ossia_constraint->addTimeProcess(plug->process());
        }
    }
}

void OSSIAConstraintElement::on_processRemoved(const Process& process)
{
    auto it = m_processes.find(process.id());
    if(it != m_processes.end())
    {
        // It is possible for a process to be null
        // (e.g. invalid state in GUI like automation without address)
        auto proc = (*it).second->process();
        auto proc_it =  std::find(m_ossia_constraint->timeProcesses().begin(),
                                  m_ossia_constraint->timeProcesses().end(),
                                  proc);
        if(proc && proc_it != m_ossia_constraint->timeProcesses().end())
            m_ossia_constraint->removeTimeProcess(proc);

        // We don't have ownership so we don't delete. The ProcessModel has it.
        m_processes.erase(it);
    }
}
