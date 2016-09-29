#include <Process/ProcessList.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>

#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>

#include <QDataStream>
#include <QtGlobal>
#include <algorithm>

#include "ClearConstraint.hpp"
#include <Process/Process.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/MapCopy.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/EntityMap.hpp>

namespace Scenario
{
namespace Command
{
ClearConstraint::ClearConstraint(const ConstraintModel& constraint) :
    m_constraintSaveData {constraint}
{
}

void ClearConstraint::undo() const
{
    auto& constraint = m_constraintSaveData.constraintPath.find();

    m_constraintSaveData.reload(constraint);
}

void ClearConstraint::redo() const
{
    auto& constraint = m_constraintSaveData.constraintPath.find();

    // We make copies since the iterators might change.
    // TODO check if this is still valid wrt boost::multi_index
    auto processes = shallow_copy(constraint.processes);
    for(auto process : processes)
    {
        RemoveProcess(constraint, process->id());
    }

    auto rackes = shallow_copy(constraint.racks);
    for(const auto& rack : rackes)
    {
        constraint.racks.remove(rack->id());
    }
}

void ClearConstraint::serializeImpl(DataStreamInput& s) const
{
    s << m_constraintSaveData;
}

void ClearConstraint::deserializeImpl(DataStreamOutput& s)
{
    s >> m_constraintSaveData;
}
}
}
