#include "BaseElementModel.hpp"
#include "BaseScenario/BaseScenario.hpp"

#include "source/Document/Constraint/ConstraintModel.hpp"
#include "source/Document/Event/EventModel.hpp"
#include "source/Document/TimeNode/TimeNodeModel.hpp"
#include "source/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"

#include <iscore/document/DocumentInterface.hpp>

#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Commands/Constraint/AddRackToConstraint.hpp"
#include "Commands/Constraint/Rack/AddSlotToRack.hpp"
#include "Commands/Constraint/Rack/Slot/ResizeSlotVertically.hpp"
#include "Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp"
#include "Commands/Scenario/ShowRackInViewModel.hpp"

#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "ProcessInterface/Process.hpp"
#include "ProcessInterface/LayerModel.hpp"

#include <QApplication>
using namespace Scenario;

#include <core/document/Document.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include "Control/ScenarioControl.hpp"
#include <iscore/presenter/PresenterInterface.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>


BaseElementModel::BaseElementModel(QObject* parent) :
    iscore::DocumentDelegateModelInterface {
        Id<iscore::DocumentDelegateModelInterface>(getNextId()),
        "BaseElementModel",
        parent},
    m_baseScenario{new BaseScenario{Id<BaseScenario>{0}, this}}
{
    initializeNewDocument(m_baseScenario->baseConstraint().fullView());

    // Help for the FocusDispatcher.
    connect(this, &BaseElementModel::setFocusedPresenter,
            &m_focusManager, &ProcessFocusManager::setFocusedPresenter);

    con(m_focusManager, &ProcessFocusManager::sig_defocusedViewModel,
            this, &BaseElementModel::on_viewModelDefocused);
    con(m_focusManager, &ProcessFocusManager::sig_focusedViewModel,
            this, &BaseElementModel::on_viewModelFocused);
}

void BaseElementModel::initializeNewDocument(const FullViewConstraintViewModel *viewmodel)
{
    using namespace Scenario::Command;
    const auto& constraint_model = viewmodel->model();

    AddProcessToConstraint cmd1{
        iscore::IDocument::path(m_baseScenario->baseConstraint()),
        "Scenario"
    };
    cmd1.redo();

    AddRackToConstraint cmd2 {
        iscore::IDocument::path(m_baseScenario->baseConstraint())
    };
    cmd2.redo();
    auto& rack = *constraint_model.racks.begin();

    ShowRackInViewModel cmd3 {
        iscore::IDocument::path(static_cast<const ConstraintViewModel&>(*viewmodel)),
        rack.id() };
    cmd3.redo();

    AddSlotToRack cmd4 {
        iscore::IDocument::path(*m_baseScenario->baseConstraint().racks.begin()),
    };
    cmd4.redo();

    ResizeSlotVertically cmd5
    {
        iscore::IDocument::path(*m_baseScenario->baseConstraint().racks.begin()->slotmodels.begin()),
        1500
    };
    cmd5.redo();

    AddLayerModelToSlot cmd6
    {
        iscore::IDocument::path(*m_baseScenario->baseConstraint().racks.begin()->slotmodels.begin()),
        iscore::IDocument::path(*m_baseScenario->baseConstraint().processes.begin())
    };
    cmd6.redo();
}
ConstraintModel& BaseElementModel::baseConstraint() const
{
    return m_baseScenario->baseConstraint();
}

static void updateSlotFocus(const LayerModel* lm, bool b)
{
    if(lm && lm->parent())
    {
        if(auto slot = dynamic_cast<SlotModel*>(lm->parent()))
        {
            slot->setFocus(b);
        }
    }
}

void BaseElementModel::on_viewModelDefocused(const LayerModel* vm)
{
    // Disable the focus on previously focused view model
    updateSlotFocus(vm, false);

    // Deselect
    iscore::SelectionDispatcher selectionDispatcher(
                iscore::IDocument::documentFromObject(*this)->selectionStack());
    selectionDispatcher.setAndCommit(Selection{});
}

void BaseElementModel::on_viewModelFocused(const LayerModel* process)
{
    // Enable focus on the new viewmodel
    updateSlotFocus(process, true);

    // If the parent of the layer is a constraint, we set the focus on the constraint too.
    auto slot = process->parent();
    if(!slot) return;
    auto rack = slot->parent();
    if(!rack) return;
    auto cm = rack->parent();
    if(auto constraint = dynamic_cast<ConstraintModel*>(cm))
    {
        if(m_focusedConstraint)
            m_focusedConstraint->focusChanged(false);

        m_focusedConstraint = constraint;
        m_focusedConstraint->focusChanged(true);
    }

}

// TODO candidate for ProcessSelectionManager.
void BaseElementModel::setNewSelection(const Selection& s)
{
    auto process = m_focusManager.focusedModel();

    // Manages the selection (different case if we're
    // selecting something in a process, or something in full view)
    if(s.empty())
    {
        if(process)
        {
            process->setSelection(Selection{});
        }

        displayedElements.setSelection(Selection{});
        // Note : once here was a call to defocus a presenter. Why ? See git blame.
    }
    else if(std::any_of(s.begin(),
                        s.end(),
                        [&] (const QObject* obj)
    {
        return obj == &displayedElements.displayedConstraint()
            || obj == &displayedElements.startNode()
            || obj == &displayedElements.endNode()
            || obj == &displayedElements.startEvent()
            || obj == &displayedElements.endEvent()
            || obj == &displayedElements.startState()
            || obj == &displayedElements.endState();
    }))
    {
        if(process)
        {
            process->setSelection(Selection{});
        }

        displayedElements.setSelection(s);
        m_focusManager.focusNothing();
    }
    else
    {
        displayedElements.setSelection(Selection{});

        // We know by the presenter that all objects
        // in a given selection are in the same Process.
        auto newProc = parentProcess(*s.begin());
        if(process && newProc != process)
        {
            process->setSelection(Selection{});
        }

        if(newProc)
        {
            newProc->setSelection(s);
        }
    }

    emit focusMe();
}

void BaseElementModel::setDisplayedConstraint(const ConstraintModel& constraint)
{
    if(&constraint == &displayedElements.displayedConstraint())
        return;

    displayedElements.setDisplayedConstraint(constraint);

    m_focusManager.focusNothing();

    disconnect(m_constraintConnection);
    if(&constraint != &m_baseScenario->baseConstraint())
    {
        m_constraintConnection =
                connect(constraint.fullView(), &QObject::destroyed,
                        this, [&] () { setDisplayedConstraint(m_baseScenario->baseConstraint()); });
    }

    emit displayedConstraintChanged();
}
