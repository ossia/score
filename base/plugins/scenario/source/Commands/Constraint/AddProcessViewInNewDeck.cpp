#include "AddProcessViewInNewDeck.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"

#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"

#include "core/interface/document/DocumentInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

AddProcessViewInNewDeck::AddProcessViewInNewDeck():
	SerializableCommand{"ScenarioControl",
                        "AddProcessViewInNewDeck",
                        QObject::tr("Add process view in new deck")}
{
}

AddProcessViewInNewDeck::AddProcessViewInNewDeck(ObjectPath&& constraintPath, QString process):
	SerializableCommand{"ScenarioControl",
                        "AddProcessViewInNewDeck",
                        QObject::tr("Add process view in new deck")},
	m_path{std::move(constraintPath)},
	m_processName{process}
{
    auto constraint = m_path.find<ConstraintModel>();
    if(constraint->boxes().size() == 0)
    {
        m_createdBoxId = getStrongId(constraint->boxes());
        m_existingBox = false;
    }
    else
    {
        m_createdBoxId = constraint->boxes().back()->id();
        m_existingBox = true;
    }
    m_createdDeckId = id_type<DeckModel>(getNextId());
    m_createdProcessViewId = id_type<ProcessViewModelInterface>(getNextId());
    m_sharedProcessModelId = id_type<ProcessSharedModelInterface>{m_processName.toInt()};
	m_processPath = IDocument::path(constraint->process(m_sharedProcessModelId));
}

void AddProcessViewInNewDeck::undo()
{
    auto constraint = m_path.find<ConstraintModel>();
    auto box = constraint->box(m_createdBoxId);
    auto deck = box->deck(m_createdDeckId);

    // Process view
    deck->deleteProcessViewModel(m_createdProcessViewId);
    // DECK
    box->removeDeck(m_createdDeckId);
    // BOX
    if (!m_existingBox)
            constraint->removeBox(m_createdBoxId);
}

void AddProcessViewInNewDeck::redo()
{
    auto constraint = m_path.find<ConstraintModel>();
    // BOX
    if (!m_existingBox)
    {
        constraint->createBox(m_createdBoxId);

        // If it is the first box created,
        // it is also assigned to the full view of the constraint.
        if(constraint->boxes().size() == 1)
        {
            constraint->fullView()->showBox(m_createdBoxId);
        }
    }

    //DECK
    auto box = constraint->box(m_createdBoxId);
	box->addDeck(new DeckModel{m_createdDeckId,
							   box});

    // Process View
    auto deck = box->deck(m_createdDeckId);
    auto proc = m_processPath.find<ProcessSharedModelInterface>();

    deck->addProcessViewModel(proc->makeViewModel(m_createdProcessViewId, deck));
}

int AddProcessViewInNewDeck::id() const
{
	return 1;
}

bool AddProcessViewInNewDeck::mergeWith(const QUndoCommand* other)
{
	return false;
}

void AddProcessViewInNewDeck::serializeImpl(QDataStream& s) const
{
    s << m_path << m_processPath << m_processName << m_existingBox << m_processId << m_createdBoxId << m_createdDeckId << m_createdProcessViewId << m_sharedProcessModelId;
}

void AddProcessViewInNewDeck::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_processPath >> m_processName >> m_existingBox >> m_processId >> m_createdBoxId >> m_createdDeckId >> m_createdProcessViewId >> m_sharedProcessModelId;
}
