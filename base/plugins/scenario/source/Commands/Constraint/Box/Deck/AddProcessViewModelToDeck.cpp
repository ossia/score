#include "AddProcessViewModelToDeck.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include "ProcessInterfaceSerialization/ProcessViewModelInterfaceSerialization.hpp"

using namespace iscore;
using namespace Scenario::Command;

AddProcessViewModelToDeck::AddProcessViewModelToDeck() :
    SerializableCommand {"ScenarioControl",
    "AddProcessViewModelToDeck",
    QObject::tr("Add process view")
}
{
}

AddProcessViewModelToDeck::AddProcessViewModelToDeck(
    ObjectPath&& deckPath,
    ObjectPath&& processPath) :
    SerializableCommand {"ScenarioControl",
    "AddProcessViewModelToDeck",
    QObject::tr("Add process view")
},
m_deckPath {deckPath},
m_processPath {processPath}
{
    auto deck = m_deckPath.find<DeckModel>();
    m_createdProcessViewId = getStrongId(deck->processViewModels());
}

void AddProcessViewModelToDeck::undo()
{
    auto deck = m_deckPath.find<DeckModel>();
    deck->deleteProcessViewModel(m_createdProcessViewId);
}

void AddProcessViewModelToDeck::redo()
{
    auto deck = m_deckPath.find<DeckModel>();
    auto proc = m_processPath.find<ProcessSharedModelInterface>();

    deck->addProcessViewModel(proc->makeViewModel(m_createdProcessViewId, deck));
}

int AddProcessViewModelToDeck::id() const
{
    return 1;
}

bool AddProcessViewModelToDeck::mergeWith(const QUndoCommand* other)
{
    return false;
}

void AddProcessViewModelToDeck::serializeImpl(QDataStream& s) const
{
    s << m_deckPath << m_processPath << m_createdProcessViewId;
}

void AddProcessViewModelToDeck::deserializeImpl(QDataStream& s)
{
    s >> m_deckPath >> m_processPath >> m_createdProcessViewId;
}
