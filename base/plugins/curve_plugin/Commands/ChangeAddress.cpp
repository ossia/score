#include "ChangeAddress.hpp"
#include <Automation/AutomationModel.hpp>
using namespace iscore;
#define CMD_UID 2001
#define CMD_NAME "ChangeAddress"
#define CMD_DESC QObject::tr("Change Curve address")

ChangeAddress::ChangeAddress():
	SerializableCommand{"AutomationControl",
						CMD_NAME,
						CMD_DESC}
{
}

ChangeAddress::ChangeAddress(ObjectPath&& path,
							 QString addr):
	SerializableCommand{"AutomationControl",
						CMD_NAME,
						CMD_DESC},
	m_path{path},
	m_newAddr{addr}
{
	auto autom = m_path.find<AutomationModel>();
	m_oldAddr = autom->address();
}

void ChangeAddress::undo()
{
	auto autom = m_path.find<AutomationModel>();
	autom->setAddress(m_oldAddr);
}

void ChangeAddress::redo()
{
	auto autom = m_path.find<AutomationModel>();
	autom->setAddress(m_newAddr);
}

int ChangeAddress::id() const
{
	return CMD_UID;
}

bool ChangeAddress::mergeWith(const QUndoCommand* other)
{
	return false;
}

void ChangeAddress::serializeImpl(QDataStream& s) const
{
	s << m_path << m_oldAddr << m_newAddr;
}

void ChangeAddress::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_oldAddr >> m_newAddr;
}
