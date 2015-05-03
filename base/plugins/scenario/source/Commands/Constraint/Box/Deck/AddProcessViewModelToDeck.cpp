#include "AddProcessViewModelToDeck.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/ProcessViewModel.hpp"
#include "ProcessInterfaceSerialization/ProcessViewModelSerialization.hpp"

using namespace iscore;
using namespace Scenario::Command;

AddProcessViewModelToDeck::AddProcessViewModelToDeck(
        ObjectPath&& deckPath,
        ObjectPath&& processPath) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_deckPath {deckPath},
    m_processPath {processPath}
{
    auto& deck = m_deckPath.find<DeckModel>();
    m_createdProcessViewId = getStrongId(deck.processViewModels());
    m_processData = m_processPath.find<ProcessModel>().makeViewModelConstructionData();
}

void AddProcessViewModelToDeck::undo()
{
    auto& deck = m_deckPath.find<DeckModel>();
    deck.deleteProcessViewModel(m_createdProcessViewId);
}

void AddProcessViewModelToDeck::redo()
{
    auto& deck = m_deckPath.find<DeckModel>();
    auto& proc = m_processPath.find<ProcessModel>();

    deck.addProcessViewModel(proc.makeViewModel(m_createdProcessViewId, m_processData, &deck));
}

void AddProcessViewModelToDeck::serializeImpl(QDataStream& s) const
{
    s << m_deckPath << m_processPath << m_processData << m_createdProcessViewId;
}

void AddProcessViewModelToDeck::deserializeImpl(QDataStream& s)
{
    s >> m_deckPath >> m_processPath >> m_processData >> m_createdProcessViewId;
}
