#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <algorithm>

#include "SetLooping.hpp"
#include "iscore/serialization/DataStreamVisitor.hpp"
#include "iscore/tools/ModelPathSerialization.hpp"

SetLooping::SetLooping(Path<ConstraintModel>&& constraintPath, bool looping) :
    m_path {std::move(constraintPath)},
    m_looping {looping}
{
}

void SetLooping::undo() const
{
    m_path.find().setLooping(!m_looping);
}

void SetLooping::redo() const
{
    m_path.find().setLooping(m_looping);
}

void SetLooping::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_looping;
}

void SetLooping::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_looping;
}
