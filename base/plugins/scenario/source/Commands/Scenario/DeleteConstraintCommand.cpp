#include "DeleteConstraintCommand.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"

#include <core/tools/utilsCPP11.hpp>
using namespace iscore;

EmptyConstraintBoxCommand::EmptyConstraintBoxCommand():
	iscore::SerializableCommand{"ScenarioControl",
								"EmptyConstraintBoxCommand",
								QObject::tr("Clear a box")}
{

}

EmptyConstraintBoxCommand::EmptyConstraintBoxCommand(ObjectPath&& constraintPath):
	iscore::SerializableCommand{"ScenarioControl",
								"EmptyConstraintBoxCommand",
								QObject::tr("Clear a box")},
	m_path{std::move(constraintPath)}
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

	serializeVectorOfPointers(constraint->boxes(),
							  m_serializedBoxes);


	for(const auto& process : constraint->processes())
	{
		QByteArray arr;

		QDataStream s(&arr, QIODevice::WriteOnly);
		s.setVersion(QDataStream::Qt_5_3);

		s << *process;

		m_serializedProcesses.push_back(arr);
	}
}

void EmptyConstraintBoxCommand::undo()
{
	qDebug(Q_FUNC_INFO);
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

void EmptyConstraintBoxCommand::redo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

	// Deleting all the processes is enough : since the storeys become empty,
	// they get deleted, and since the boxes become empty, they get deleted too.

	// We make a copy because the elements get erased of the original vector.
	auto processesToDelete = constraint->processes();
	for(auto& process : processesToDelete)
	{
		constraint->removeProcess(process->id());
	}

	auto boxesToDelete = constraint->boxes();
	for(auto& box : boxesToDelete)
	{
		constraint->removeBox(box->id());
	}
}

int EmptyConstraintBoxCommand::id() const
{
	return 1;
}

bool EmptyConstraintBoxCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}


// TODO
void EmptyConstraintBoxCommand::serializeImpl(QDataStream& s)
{
	s << m_path << m_serializedBoxes << m_serializedProcesses;
}

void EmptyConstraintBoxCommand::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_serializedBoxes >> m_serializedProcesses;
}
