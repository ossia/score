#include "CopyProcessViewModel.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

CopyProcessViewModel::CopyProcessViewModel(ObjectPath&& pvmPath,
        ObjectPath&& targetDeckPath) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_pvmPath {pvmPath},
    m_targetDeckPath {targetDeckPath}
{
    auto deck = m_targetDeckPath.find<DeckModel>();
    m_newProcessViewModelId = getStrongId(deck->processViewModels());
}

void CopyProcessViewModel::undo()
{
    auto deck = m_targetDeckPath.find<DeckModel>();
    deck->deleteProcessViewModel(m_newProcessViewModelId);
}

void CopyProcessViewModel::redo()
{
    auto sourcePVM = m_pvmPath.find<ProcessViewModelInterface>();
    auto targetDeck = m_targetDeckPath.find<DeckModel>();

    auto proc = sourcePVM->sharedProcessModel();
    targetDeck->addProcessViewModel(
        proc->makeViewModel(m_newProcessViewModelId,
                            sourcePVM,
                            targetDeck));
}

int CopyProcessViewModel::id() const
{
    return 1;
}

bool CopyProcessViewModel::mergeWith(const QUndoCommand* other)
{
    return false;
}

void CopyProcessViewModel::serializeImpl(QDataStream& s) const
{
    s << m_pvmPath << m_targetDeckPath << m_newProcessViewModelId;
}

void CopyProcessViewModel::deserializeImpl(QDataStream& s)
{
    s >> m_pvmPath >> m_targetDeckPath >> m_newProcessViewModelId;
}
