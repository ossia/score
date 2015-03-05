#include "MovePoint.hpp"
#include <Automation/AutomationModel.hpp>
using namespace iscore;
#define CMD_UID 2002
#define CMD_NAME "MovePoint"
#define CMD_DESC QObject::tr("Move point from curve")

MovePoint::MovePoint() :
    SerializableCommand {"AutomationControl",
    CMD_NAME,
    CMD_DESC
}
{
}

MovePoint::MovePoint(ObjectPath&& path,
                     double oldx, double newx, double newy) :
    SerializableCommand {"AutomationControl",
    CMD_NAME,
    CMD_DESC
},
m_path {path},
m_oldX {oldx},
m_newX {newx},
m_newY {newy}
{
    auto autom = m_path.find<AutomationModel>();
    m_oldY = autom->points() [oldx];
}

void MovePoint::undo()
{
    auto autom = m_path.find<AutomationModel>();
    autom->movePoint(m_newX, m_oldX, m_oldY);
}

void MovePoint::redo()
{
    auto autom = m_path.find<AutomationModel>();
    autom->movePoint(m_oldX, m_newX, m_newY);
}

bool MovePoint::mergeWith(const Command* other)
{
    return false;
}

void MovePoint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_oldX << m_oldY << m_newX << m_newY;
}

void MovePoint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_oldX >> m_oldY >> m_newX >> m_newY;
}
