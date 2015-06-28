#include "OSSIAScenarioElement.hpp"

#include <API/Headers/Editor/Scenario.h>
#include <API/Headers/Editor/TimeConstraint.h>
#include <API/Headers/Editor/TimeEvent.h>
#include <API/Headers/Editor/TimeNode.h>

#include <Process/ScenarioModel.hpp>
OSSIAScenarioElement::OSSIAScenarioElement(const ScenarioModel* element, QObject* parent):
    m_iscore_scenario{element}
{
    m_ossia_scenario = OSSIA::Scenario::create();

    connect(element, &ScenarioModel::constraintCreated,
            this, &OSSIAScenarioElement::on_constraintCreated);
    connect(element, &ScenarioModel::eventCreated,
            this, &OSSIAScenarioElement::on_eventCreated);
    connect(element, &ScenarioModel::timeNodeCreated,
            this, &OSSIAScenarioElement::on_timeNodeCreated);

    connect(element, &ScenarioModel::constraintMoved,
            this, &OSSIAScenarioElement::on_constraintMoved);
    connect(element, &ScenarioModel::eventMoved,
            this, &OSSIAScenarioElement::on_eventMoved);

    connect(element, &ScenarioModel::constraintRemoved,
            this, &OSSIAScenarioElement::on_constraintRemoved);
    connect(element, &ScenarioModel::eventRemoved,
            this, &OSSIAScenarioElement::on_eventRemoved);
    connect(element, &ScenarioModel::timeNodeRemoved,
            this, &OSSIAScenarioElement::on_timeNodeRemoved);

    // Create elements for the existing stuff. (e.g. start/ end timenode / event)
    for(auto& timenode : m_iscore_scenario->timeNodes())
    {
        on_timeNodeCreated(timenode->id());
    }
    for(auto& event : m_iscore_scenario->events())
    {
        on_eventCreated(event->id());
    }
    for(auto& constraint : m_iscore_scenario->constraints())
    {
        on_constraintCreated(constraint->id());
    }
}



iscore::ElementPluginModel* OSSIAScenarioElement::clone(
        const QObject* element,
        QObject* parent) const
{
    qDebug() << Q_FUNC_INFO << "TODO";
    return nullptr;
}


iscore::ElementPluginModelType OSSIAScenarioElement::elementPluginId() const
{
    return staticPluginId();
}

void OSSIAScenarioElement::serialize(const VisitorVariant&) const
{
    qDebug() << Q_FUNC_INFO << "TODO";
}

void OSSIAScenarioElement::on_constraintCreated(const id_type<ConstraintModel>& id)
{
    qDebug(Q_FUNC_INFO);

}

void OSSIAScenarioElement::on_eventCreated(const id_type<EventModel>& id)
{
    auto& ev = m_iscore_scenario->event(id);

    Q_ASSERT(m_ossia_timenodes.find(ev.timeNode()) != m_ossia_timenodes.end());
    auto ossia_tn = m_ossia_timenodes.at(ev.timeNode());

    std::shared_ptr<OSSIA::TimeEvent> ossia_ev;
    // If it's the first, it's already here
    if(ossia_tn->timeNode()->timeEvents().size() == 1)
    {
        ossia_ev = ossia_tn->timeNode()->timeEvents().front();
    }
    else
    {
        ossia_ev = *ossia_tn->timeNode()->emplace(ossia_tn->timeNode()->timeEvents().begin());
    }

    // Pass the element in ctor
    auto elt = new OSSIAEventElement(ossia_ev, &ev, &ev);

    ev.pluginModelList.add(elt);
    m_ossia_timeevents.insert({id, elt});
    // The OSSIA timenodes already have the event.

}

void OSSIAScenarioElement::on_timeNodeCreated(const id_type<TimeNodeModel>& id)
{
    auto& tn = m_iscore_scenario->timeNode(id);
    auto elt = new OSSIATimeNodeElement(&tn, &tn);

    tn.pluginModelList.add(elt);
    m_ossia_timenodes.insert({id, elt});
}

void OSSIAScenarioElement::on_constraintMoved(const id_type<ConstraintModel>& id)
{
    qDebug(Q_FUNC_INFO);

}

void OSSIAScenarioElement::on_eventMoved(const id_type<EventModel>& id)
{
    qDebug(Q_FUNC_INFO);

}

void OSSIAScenarioElement::on_constraintRemoved(const id_type<ConstraintModel>& id)
{
    qDebug(Q_FUNC_INFO);

}

void OSSIAScenarioElement::on_eventRemoved(const id_type<EventModel>& id)
{
    qDebug(Q_FUNC_INFO);

}

void OSSIAScenarioElement::on_timeNodeRemoved(const id_type<TimeNodeModel>& id)
{
    qDebug(Q_FUNC_INFO);

}
