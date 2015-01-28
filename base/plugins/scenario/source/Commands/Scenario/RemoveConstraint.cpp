#include "RemoveConstraint.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "source/ProcessInterfaceSerialization/ProcessSharedModelInterfaceSerialization.hpp"

#include <core/tools/utilsCPP11.hpp>

using namespace iscore;
using namespace Scenario::Command;

RemoveConstraint::RemoveConstraint():
	SerializableCommand{"ScenarioControl",
                        "RemoveConstraint",
						QObject::tr("Remove event and pre-constraints")}
{
}


RemoveConstraint::RemoveConstraint(ObjectPath&& scenarioPath, ConstraintModel* constraint):
	SerializableCommand{"ScenarioControl",
                        "RemoveConstraint",
						QObject::tr("Remove event and pre-constraints")},
    m_path{std::move(scenarioPath)}
{
    QByteArray arr;
    Serializer<DataStream> s{&arr};
    s.readFrom(*constraint);
    m_serializedConstraint = arr;

    m_cstrId = constraint->id();
}

void RemoveConstraint::undo()
{

}

void RemoveConstraint::redo()
{
    auto scenar = m_path.find<ScenarioProcessSharedModel>();
    scenar->removeConstraint(m_cstrId);
}

int RemoveConstraint::id() const
{
	return 1;
}

bool RemoveConstraint::mergeWith(const QUndoCommand* other)
{
	return false;
}

void RemoveConstraint::serializeImpl(QDataStream& s)
{
    s << m_path ;
}

void RemoveConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path ;
}
