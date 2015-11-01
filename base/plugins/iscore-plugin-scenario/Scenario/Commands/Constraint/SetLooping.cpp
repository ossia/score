#include "SetLooping.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
SetLooping::SetLooping(Path<ConstraintModel>&& constraintPath, bool looping) :
    SerializableCommand {factoryName(),
                         commandName(),
                         description()},
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

void SetLooping::serializeImpl(QDataStream& s) const
{
    s << m_path << m_looping;
}

void SetLooping::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_looping;
}
