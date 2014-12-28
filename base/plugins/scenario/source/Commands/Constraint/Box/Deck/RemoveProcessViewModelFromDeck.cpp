#include "RemoveProcessViewModelFromDeck.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterfaceSerialization.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveProcessViewModelFromDeck::RemoveProcessViewModelFromDeck(ObjectPath&& boxPath,
													 int processViewId):
	SerializableCommand{"ScenarioControl",
						"RemoveProcessViewFromDeck",
						"Remove process view"},
	m_path{boxPath},
	m_processViewId{processViewId}
{
	auto deck = static_cast<DeckModel*>(m_path.find());

	Serializer<DataStream> s{&m_serializedProcessViewData};
	s.visit(static_cast<const ProcessViewModelInterface&>(*deck->processViewModel(m_processViewId)));
}

void RemoveProcessViewModelFromDeck::undo()
{
	auto deck = static_cast<DeckModel*>(m_path.find());
	Deserializer<DataStream> s{&m_serializedProcessViewData};
	auto pvm = createProcessViewModel(s,
									  deck->parentConstraint(),
									  deck);
	deck->addProcessViewModel(pvm);
}

void RemoveProcessViewModelFromDeck::redo()
{
	auto deck = static_cast<DeckModel*>(m_path.find());
	deck->deleteProcessViewModel(m_processViewId);
}

int RemoveProcessViewModelFromDeck::id() const
{
	return 1;
}

bool RemoveProcessViewModelFromDeck::mergeWith(const QUndoCommand* other)
{
	return false;
}

void RemoveProcessViewModelFromDeck::serializeImpl(QDataStream& s)
{
	s << m_path << m_processViewId << m_serializedProcessViewData;
}

void RemoveProcessViewModelFromDeck::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_processViewId >> m_serializedProcessViewData;
}
