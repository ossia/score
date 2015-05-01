#include "AddProcessViewInNewDeck.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"

#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"


using namespace iscore;
using namespace Scenario::Command;

AddProcessViewInNewDeck::AddProcessViewInNewDeck(ObjectPath&& constraintPath,
                                                 id_type<ProcessSharedModelInterface> process) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_path {std::move(constraintPath) },
    m_sharedProcessModelId{process}
{
    auto constraint = m_path.find<ConstraintModel>();

    if(constraint->boxes().empty())
    {
        m_createdBoxId = getStrongId(constraint->boxes());
        m_existingBox = false;
    }
    else
    {
        m_createdBoxId = constraint->boxes().back()->id();
        m_existingBox = true;
    }

    m_createdDeckId = id_type<DeckModel> (getNextId());
    m_createdProcessViewId = id_type<ProcessViewModelInterface> (getNextId());
}

void AddProcessViewInNewDeck::undo()
{
    auto constraint = m_path.find<ConstraintModel>();
    auto box = constraint->box(m_createdBoxId);

    // Removing the deck is enough
    box->removeDeck(m_createdDeckId);

    // Remove the box
    if(!m_existingBox)
    {
        constraint->removeBox(m_createdBoxId);
    }
}

void AddProcessViewInNewDeck::redo()
{
    auto constraint = m_path.find<ConstraintModel>();

    // BOX
    if(!m_existingBox)
    {
        constraint->createBox(m_createdBoxId);

        // If it is the first box created,
        // it is also assigned to  all the views of the constraint.
        if(constraint->boxes().size() == 1)
        {
            for(auto vm : constraint->viewModels())
            {
                vm->showBox(m_createdBoxId);
            }
        }
    }

    //DECK
    auto box = constraint->box(m_createdBoxId);
    box->addDeck(new DeckModel {m_createdDeckId,
                                box
                 });

    // Process View
    auto deck = box->deck(m_createdDeckId);
    auto proc = constraint->process(m_sharedProcessModelId);

    deck->addProcessViewModel(proc->makeViewModel(m_createdProcessViewId, deck));
}

void AddProcessViewInNewDeck::serializeImpl(QDataStream& s) const
{
    s << m_path << m_existingBox << m_processId << m_createdBoxId << m_createdDeckId << m_createdProcessViewId << m_sharedProcessModelId;
}

void AddProcessViewInNewDeck::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_existingBox >> m_processId >> m_createdBoxId >> m_createdDeckId >> m_createdProcessViewId >> m_sharedProcessModelId;
}
