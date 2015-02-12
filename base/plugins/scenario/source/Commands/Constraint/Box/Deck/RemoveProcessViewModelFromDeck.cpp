#include "RemoveProcessViewModelFromDeck.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include "source/ProcessInterfaceSerialization/ProcessViewModelInterfaceSerialization.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveProcessViewModelFromDeck::RemoveProcessViewModelFromDeck():
	SerializableCommand{"ScenarioControl",
						"RemoveProcessViewModelFromDeck",
						QObject::tr("Remove process view")}
{
}

RemoveProcessViewModelFromDeck::RemoveProcessViewModelFromDeck(ObjectPath &&pvmPath):
	SerializableCommand{"ScenarioControl",
						"RemoveProcessViewModelFromDeck",
						QObject::tr("Remove process view")}
{
	auto deckPath = pvmPath.vec();
	auto lastId = deckPath.takeLast();
	m_path = ObjectPath{std::move(deckPath)};
	m_processViewId = id_type<ProcessViewModelInterface>(lastId.id());
}

RemoveProcessViewModelFromDeck::RemoveProcessViewModelFromDeck(
										ObjectPath&& boxPath,
										id_type<ProcessViewModelInterface> processViewId):
	SerializableCommand{"ScenarioControl",
						"RemoveProcessViewModelFromDeck",
						QObject::tr("Remove process view")},
	m_path{boxPath},
	m_processViewId{processViewId}
{
	auto deck = m_path.find<DeckModel>();

	Serializer<DataStream> s{&m_serializedProcessViewData};
	s.readFrom(*deck->processViewModel(m_processViewId));
}

void RemoveProcessViewModelFromDeck::undo()
{
	auto deck = m_path.find<DeckModel>();
	Deserializer<DataStream> s{&m_serializedProcessViewData};

	auto pvm = createProcessViewModel(s,
									  deck->parentConstraint(),
									  deck);
	deck->addProcessViewModel(pvm);
}

void RemoveProcessViewModelFromDeck::redo()
{
	auto deck = m_path.find<DeckModel>();
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

void RemoveProcessViewModelFromDeck::serializeImpl(QDataStream& s) const
{
	s << m_path << m_processViewId << m_serializedProcessViewData;
}

void RemoveProcessViewModelFromDeck::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_processViewId >> m_serializedProcessViewData;
}
