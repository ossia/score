#include "AddPoint.hpp"
#include <Automation/AutomationModel.hpp>
using namespace iscore;
#define CMD_UID 2000
#define CMD_NAME "AddPoint"
#define CMD_DESC QObject::tr("Add point to curve")

AddPoint::AddPoint() :
    SerializableCommand {"AutomationControl",
    CMD_NAME,
    CMD_DESC
}
{
}

AddPoint::AddPoint (ObjectPath&& path,
                    double x, double y) :
    SerializableCommand {"AutomationControl",
    CMD_NAME,
    CMD_DESC
},
m_path {path},
m_x {x},
m_y {y}
{
}

void AddPoint::undo()
{
    auto autom = m_path.find<AutomationModel>();
    autom->removePoint (m_x);
}

void AddPoint::redo()
{
    auto autom = m_path.find<AutomationModel>();
    autom->addPoint (m_x, m_y);
}

int AddPoint::id() const
{
    return CMD_UID;
}

bool AddPoint::mergeWith (const QUndoCommand* other)
{
    return false;
}

void AddPoint::serializeImpl (QDataStream& s) const
{
    s << m_path << m_x << m_y;
}

void AddPoint::deserializeImpl (QDataStream& s)
{
    s >> m_path >> m_x >> m_y;
}
