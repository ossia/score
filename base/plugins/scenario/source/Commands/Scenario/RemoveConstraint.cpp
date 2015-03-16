#include "RemoveConstraint.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "source/ProcessInterfaceSerialization/ProcessSharedModelInterfaceSerialization.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"
#include <iscore/tools/utilsCPP11.hpp>
#include "Document/Constraint/ViewModels/AbstractConstraintViewModelSerialization.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveConstraint::RemoveConstraint(ObjectPath&& scenarioPath, ConstraintModel* constraint) :
    SerializableCommand{"ScenarioControl",
                        className(),
                        description()},
m_path {std::move(scenarioPath) }
{
    Serializer<DataStream> cstReader{&m_serializedConstraint};
    cstReader.readFrom(*constraint);

    m_cstrId = constraint->id();

    auto scenar = m_path.find<ScenarioModel>();
    // We have to backup all the view models pointing to a constraint.
    // The full view is already back-upped by the serialization process.

    // The other constraint view models are in their respective scenario view models
    for(auto& viewModel : viewModels(scenar))
    {
        // TODO we need to know its concrete type in order to serialize it correctly.
        auto cstrVM = viewModel->constraint(constraint->id());
        if(auto temporalCstrVM = dynamic_cast<TemporalConstraintViewModel*>(cstrVM))
        {
            auto pvm_id = identifierOfViewModelFromSharedModel(viewModel);

            QByteArray arr;
            Serializer<DataStream> cvmReader{&arr};
            cvmReader.readFrom(*temporalCstrVM);

            m_serializedConstraintViewModels[pvm_id] = {"Temporal", arr};
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "TODO";
        }
    }
}

void RemoveConstraint::undo()
{
    auto scenar = m_path.find<ScenarioModel>();

    Deserializer<DataStream> s {&m_serializedConstraint};

    auto newConstraint = new ConstraintModel(s, scenar);
    scenar->addConstraint(newConstraint);

    scenar->event(newConstraint->startEvent())->addNextConstraint(newConstraint->id());
    scenar->event(newConstraint->endEvent())->addPreviousConstraint(newConstraint->id());


    for(auto& viewModel : viewModels(scenar))
    {
        if(TemporalScenarioViewModel* temporalSVM = dynamic_cast<TemporalScenarioViewModel*>(viewModel))
        {
            auto cvm_id = identifierOfViewModelFromSharedModel(temporalSVM);

            if(m_serializedConstraintViewModels.contains(cvm_id))
            {
                Deserializer<DataStream> d(&m_serializedConstraintViewModels[cvm_id].second);
                auto cstr = createConstraintViewModel(d, temporalSVM);
                temporalSVM->addConstraintViewModel(cstr);
            }
            else
            {
                throw std::runtime_error("undo RemoveConstraint : missing identifier.");
            }
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "TODO";
        }
    }
}

void RemoveConstraint::redo()
{
    auto scenar = m_path.find<ScenarioModel>();
    StandardRemovalPolicy::removeConstraint(*scenar, m_cstrId);
}

bool RemoveConstraint::mergeWith(const Command* other)
{
    return false;
}

void RemoveConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_cstrId << m_serializedConstraint << m_serializedConstraintViewModels << m_constraintFullViewId;
}

void RemoveConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_cstrId >> m_serializedConstraint >> m_serializedConstraintViewModels >> m_constraintFullViewId;
}
