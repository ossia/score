#include "RemovePoint.hpp"
#include <Automation/AutomationModel.hpp>
using namespace iscore;
#define CMD_UID 2003
#define CMD_NAME "RemovePoint"
#define CMD_DESC QObject::tr("Remove point from curve")

RemovePoint::RemovePoint() :
    SerializableCommand {"AutomationControl",
    CMD_NAME,
    CMD_DESC
}
{
}

RemovePoint::RemovePoint(ObjectPath&& path,
                         double x) :
    SerializableCommand {"AutomationControl",
    CMD_NAME,
    CMD_DESC
},
m_path {path},
m_x {x}
{
    auto& autom = m_path.find<AutomationModel>();
    m_oldY = autom.points() [x];
}

void RemovePoint::undo()
{
    auto& autom = m_path.find<AutomationModel>();
    autom.addPoint(m_x, m_oldY);
}

void RemovePoint::redo()
{
    auto& autom = m_path.find<AutomationModel>();
    autom.removePoint(m_x);
}

void RemovePoint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_x << m_oldY;
}

void RemovePoint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_x >> m_oldY;
}
