#include "CopyBox.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

CopyBox::CopyBox():
	SerializableCommand{"ScenarioControl",
						"CopyBox",
						QObject::tr("Copy a box")}
{
}

CopyBox::CopyBox(ObjectPath&& boxToCopy):
	SerializableCommand{"ScenarioControl",
						"CopyDeck",
						QObject::tr("Copy a box")},
	m_boxPath{boxToCopy}
{
	auto box = m_boxPath.find<BoxModel>();
	auto constraint = box->constraint();

	m_newBoxId = getStrongId(constraint->boxes());
}

void CopyBox::undo()
{
	auto box = m_boxPath.find<BoxModel>();
	auto constraint = box->constraint();

	constraint->removeBox(m_newBoxId);
}

void CopyBox::redo()
{
	auto box = m_boxPath.find<BoxModel>();
	auto constraint = box->constraint();
	constraint->addBox(new BoxModel{box, m_newBoxId, constraint});
}

int CopyBox::id() const
{
	return 1;
}

bool CopyBox::mergeWith(const QUndoCommand* other)
{
	return false;
}

void CopyBox::serializeImpl(QDataStream& s) const
{
	s << m_boxPath << m_newBoxId;
}

void CopyBox::deserializeImpl(QDataStream& s)
{
	s >> m_boxPath >> m_newBoxId;
}
