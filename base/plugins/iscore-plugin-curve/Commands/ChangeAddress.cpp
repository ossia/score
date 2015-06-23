#include "ChangeAddress.hpp"
#include <Automation/AutomationModel.hpp>
using namespace iscore;

ChangeAddress::ChangeAddress(ObjectPath&& path,
                             const Address& addr) :
    SerializableCommand {"AutomationControl",
                         commandName(),
                         description()},
    m_path {path},
    m_newAddr (addr)
{
    auto& autom = m_path.find<AutomationModel>();
    m_oldAddr = autom.address();
}

void ChangeAddress::undo()
{
    auto& autom = m_path.find<AutomationModel>();
    autom.setAddress(m_oldAddr);
}

void ChangeAddress::redo()
{
    auto& autom = m_path.find<AutomationModel>();
    autom.setAddress(m_newAddr);
}

void ChangeAddress::serializeImpl(QDataStream& s) const
{
    s << m_path << m_oldAddr << m_newAddr;
}

void ChangeAddress::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_oldAddr >> m_newAddr;
}
