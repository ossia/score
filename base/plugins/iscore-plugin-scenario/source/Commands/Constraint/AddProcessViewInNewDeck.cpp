#include "AddProcessViewInNewDeck.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"

#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/ProcessViewModel.hpp"


using namespace iscore;
using namespace Scenario::Command;

AddProcessViewInNewDeck::AddProcessViewInNewDeck(ObjectPath&& constraintPath,
                                                 id_type<ProcessModel> process) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_path {std::move(constraintPath) },
    m_sharedProcessModelId{process}
{
    auto& constraint = m_path.find<ConstraintModel>();

    if(constraint.boxes().empty())
    {
        m_createdBoxId = getStrongId(constraint.boxes());
        m_existingBox = false;
    }
    else
    {
        m_createdBoxId = constraint.boxes().back()->id();
        m_existingBox = true;
    }

    m_createdDeckId = id_type<DeckModel> (getNextId());
    m_createdProcessViewId = id_type<ProcessViewModel> (getNextId());
    m_processData = constraint.process(m_sharedProcessModelId)->makeViewModelConstructionData();
}

void AddProcessViewInNewDeck::undo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    auto box = constraint.box(m_createdBoxId);

    // Removing the deck is enough
    box->removeDeck(m_createdDeckId);

    // Remove the box
    if(!m_existingBox)
    {
        constraint.removeBox(m_createdBoxId);
    }
}

void AddProcessViewInNewDeck::redo()
{
    auto& constraint = m_path.find<ConstraintModel>();

    // Box
    if(!m_existingBox)
    {
        // TODO refactor with AddBoxToConstraint
        auto box = new BoxModel{m_createdBoxId, &constraint};
        constraint.addBox(box);
        box->metadata.setName(QString{"Box.%1"}.arg(constraint.boxes().size()));

        // If it is the first box created,
        // it is also assigned to all the views of the constraint.
        if(constraint.boxes().size() == 1)
        {
            for(const auto& vm : constraint.viewModels())
            {
                vm->showBox(m_createdBoxId);
            }
        }
    }

    // Deck
    auto box = constraint.box(m_createdBoxId);
    box->addDeck(new DeckModel {m_createdDeckId,
                                box});

    // Process View
    auto deck = box->deck(m_createdDeckId);
    auto proc = constraint.process(m_sharedProcessModelId);

    deck->addProcessViewModel(proc->makeViewModel(m_createdProcessViewId, m_processData, deck));
}

void AddProcessViewInNewDeck::serializeImpl(QDataStream& s) const
{
    s << m_path
      << m_existingBox
      << m_processId
      << m_createdBoxId
      << m_createdDeckId
      << m_createdProcessViewId
      << m_sharedProcessModelId
      << m_processData;
}

void AddProcessViewInNewDeck::deserializeImpl(QDataStream& s)
{
    s >> m_path
      >> m_existingBox
      >> m_processId
      >> m_createdBoxId
      >> m_createdDeckId
      >> m_createdProcessViewId
      >> m_sharedProcessModelId
      >> m_processData;
}
