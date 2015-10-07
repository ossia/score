#include "SetLooping.hpp"
#include <Document/Constraint/ConstraintModel.hpp>
SetLooping::SetLooping(Path<ConstraintModel>&& constraintPath, bool looping) :
    SerializableCommand {factoryName(),
                         commandName(),
                         description()},
    m_path {std::move(constraintPath)},
    m_looping {looping}
{
}

void SetLooping::undo()
{
    m_path.find().setLooping(!m_looping);
}

void SetLooping::redo()
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
