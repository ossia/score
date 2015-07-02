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
OSSIAConstraintElement::OSSIAConstraintElement(
        std::shared_ptr<OSSIA::TimeConstraint> ossia_cst,
        ConstraintModel& iscore_cst,
        QObject* parent):
    QObject{parent},
    m_iscore_constraint{iscore_cst},
    m_ossia_constraint{ossia_cst}
{
    connect(&iscore_cst, &ConstraintModel::processCreated,
            this, &OSSIAConstraintElement::on_processAdded);
    connect(&iscore_cst, &ConstraintModel::processRemoved,
            this, &OSSIAConstraintElement::on_processRemoved);

    // Setup updates
    // todo : should be in OSSIAConstraintElement
    connect(&iscore_cst, &ConstraintModel::defaultDurationChanged, this,
            [=] (const TimeValue& t) {
        ossia_cst->setDuration(iscore::convert::time(t));
    });
    connect(&iscore_cst, &ConstraintModel::minDurationChanged, this,
            [=] (const TimeValue& t) {
        ossia_cst->setDurationMin(iscore::convert::time(t));
    });
    connect(&iscore_cst, &ConstraintModel::maxDurationChanged, this,
            [=] (const TimeValue& t) {
        ossia_cst->setDurationMax(iscore::convert::time(t));
    });


    for(auto& process : iscore_cst.processes())
    {
        on_processAdded(process->processName(), process->id());
    }
}

std::shared_ptr<OSSIA::TimeConstraint> OSSIAConstraintElement::constraint() const
{
    return m_ossia_constraint;
}

void OSSIAConstraintElement::stop()
{
    m_ossia_constraint->stop();

    m_iscore_constraint.reset();
}

void OSSIAConstraintElement::on_processAdded(
        const QString& name,
        const id_type<ProcessModel>& id)
{
    // The DocumentPlugin creates the elements in the processes.
    auto proc = m_iscore_constraint.process(id);
    OSSIAProcessElement* plug{};
    if(auto scenar = dynamic_cast<ScenarioModel*>(proc))
    {
        plug = new OSSIAScenarioElement{scenar, this};
    }
    else if(auto autom = dynamic_cast<AutomationModel*>(proc))
    {
        plug = new OSSIAAutomationElement{autom, this};
    }

    if(plug)
    {
        m_processes.insert({id, plug});
        m_ossia_constraint->addTimeProcess(plug->process());
    }
}

void OSSIAConstraintElement::on_processRemoved(const id_type<ProcessModel>& process)
{
    auto it = m_processes.find(process);
    if(it != m_processes.end())
    {
        m_ossia_constraint->removeTimeProcess((*it).second->process());
        m_processes.erase(it);
    }
}
