#include "DeleteConstraintCommand.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"

#include <core/tools/utilsCPP11.hpp>
using namespace iscore;


EmptyConstraintBox::EmptyConstraintBox(ObjectPath&& constraintPath):
	iscore::SerializableCommand{"ScenarioControl",
								"EmptyConstraintBox",
								QObject::tr("Clear a box")},
	m_path{std::move(constraintPath)}
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

	serializeVectorOfPointers(constraint->boxes(),
							  m_serializedBoxes);
	serializeVectorOfPointers(constraint->processes(),
							  m_serializedProcesses);
}

void EmptyConstraintBox::undo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

	for(auto& serializedProcess : m_serializedProcesses)
	{
		QDataStream s(&serializedProcess, QIODevice::ReadOnly);
		constraint->createProcess(s);
	}

	for(auto& serializedBox : m_serializedBoxes)
	{
		QDataStream s(&serializedBox, QIODevice::ReadOnly);
		constraint->createBox(s);
	}
}

void EmptyConstraintBox::redo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

	for(auto& box : constraint->boxes())
	{
		constraint->deleteBox(box->id());
	}

	for(auto& process : constraint->processes())
	{
		constraint->deleteProcess(process->id());
	}
}

int EmptyConstraintBox::id() const
{
	return 1;
}

bool EmptyConstraintBox::mergeWith(const QUndoCommand* other)
{
	return false;
}


// TODO
void EmptyConstraintBox::serializeImpl(QDataStream&)
{
}

void EmptyConstraintBox::deserializeImpl(QDataStream&)
{
}
