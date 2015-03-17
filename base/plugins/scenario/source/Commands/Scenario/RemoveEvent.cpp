#include "RemoveEvent.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "source/ProcessInterfaceSerialization/ProcessSharedModelInterfaceSerialization.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"

#include <State/State.hpp>

#include <iscore/tools/utilsCPP11.hpp>

using namespace iscore;
using namespace Scenario::Command;

RemoveEvent::RemoveEvent(ObjectPath&& scenarioPath, EventModel* event) :
    SerializableCommand{"ScenarioControl",
                        className(),
                        description()},
m_path {std::move(scenarioPath) }
{
    QByteArray arr;
    Serializer<DataStream> s{&arr};
    s.readFrom(*event);
    m_serializedEvent = arr;

    m_evId = event->id();

    auto scenar = m_path.find<ScenarioModel>();
    auto timeNode = scenar->timeNode(event->timeNode());

    QByteArray arr2;
    Serializer<DataStream> s2{&arr2};
    s2.readFrom(*timeNode);
    m_serializedTimeNode = arr2;

    for (auto cstr : event->constraints())
    {
        ConstraintModel* constraint = scenar->constraint(cstr);

        QByteArray arr;
        Serializer<DataStream> s{&arr};
        s.readFrom(*constraint);
        m_serializedConstraints.push_back(arr);

        for(auto& viewModel : viewModels(scenar))
        {
            // todo : associer toutes les combinaisons de modèles de vues, pas que la première =)
            auto cstrVM = constraint->viewModels().first();
            auto cvm_id = identifierOfViewModelFromSharedModel(viewModel);
            m_constraintViewModelIDs[cvm_id] = cstrVM->id();
        }
    }
}

void RemoveEvent::undo()
{

    auto scenar = m_path.find<ScenarioModel>();

    Deserializer<DataStream> s2 {&m_serializedTimeNode};
    auto timeNode = new TimeNodeModel(s2, scenar);

    Deserializer<DataStream> s {&m_serializedEvent};
    auto event = new EventModel(s, scenar);

    timeNode->removeEvent(event->id());

    scenar->addTimeNode(timeNode);
    scenar->addEvent(event);

    timeNode->addEvent(event->id());

    // re-create constraints
    for (auto scstr : m_serializedConstraints)
    {
        Deserializer<DataStream> s{&scstr};
        auto cstr = new ConstraintModel(s, scenar);
        scenar->addConstraint(cstr);

        // adding constraint to second extremity event
        if (m_evId == cstr->endEvent())
            scenar->event(cstr->startEvent())->addNextConstraint(cstr->id());
        else
            scenar->event(cstr->endEvent())->addPreviousConstraint(cstr->id());

        // view model creation
        for(auto& viewModel : viewModels(scenar))
        {
            auto cvm_id = identifierOfViewModelFromSharedModel(viewModel);

            if(m_constraintViewModelIDs.contains(cvm_id))
            {
                viewModel->makeConstraintViewModel(cstr->id(),
                                                   m_constraintViewModelIDs[cvm_id]);
            }
            else
            {
                throw std::runtime_error("undo RemoveEvent : missing identifier.");
            }
        }
    }
}

void RemoveEvent::redo()
{
    auto scenar = m_path.find<ScenarioModel>();
    StandardRemovalPolicy::removeEvent(*scenar, m_evId);
}

bool RemoveEvent::mergeWith(const Command* other)
{
    return false;
}

void RemoveEvent::serializeImpl(QDataStream& s) const
{
    s << m_path << m_evId << m_serializedEvent << m_serializedConstraints << m_serializedTimeNode << m_constraintViewModelIDs ;
}

void RemoveEvent::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_evId >> m_serializedEvent >> m_serializedConstraints >> m_serializedTimeNode >> m_constraintViewModelIDs ;
}
