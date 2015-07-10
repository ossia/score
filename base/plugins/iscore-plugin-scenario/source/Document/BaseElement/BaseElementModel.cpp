#include "BaseElementModel.hpp"
#include "BaseScenario/BaseScenarioModel.hpp"

#include "source/Document/Constraint/ConstraintModel.hpp"
#include "source/Document/Event/EventModel.hpp"
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
#include "ProcessInterface/ProcessModel.hpp"
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
        id_type<iscore::DocumentDelegateModelInterface>(getNextId()),
        "BaseElementModel",
        parent},
    m_baseScenario{new BaseScenario{id_type<BaseScenario>{0}, this}}
{
    initializeNewDocument(m_baseScenario->baseConstraint().fullView());

    // Help for the FocusDispatcher.
    connect(this, &BaseElementModel::setFocusedPresenter,
            &m_focusManager, &ProcessFocusManager::setFocusedPresenter);

    connect(&m_focusManager, &ProcessFocusManager::sig_defocusedViewModel,
            this, &BaseElementModel::on_viewModelDefocused);
    connect(&m_focusManager, &ProcessFocusManager::sig_focusedViewModel,
            this, &BaseElementModel::on_viewModelFocused);
}

void BaseElementModel::initializeNewDocument(const FullViewConstraintViewModel *viewmodel)
{
    using namespace Scenario::Command;
    const auto& constraint_model = viewmodel->model();

    AddProcessToConstraint cmd1
    {
        {
            {"BaseElementModel", this->id()},
            {"BaseScenario", m_baseScenario->id()},
            {"BaseConstraintModel", {}}
        },
        "Scenario"
    };
    cmd1.redo();
    auto scenarioId = (*constraint_model.processes().begin())->id();

    AddRackToConstraint cmd2
    {
        ObjectPath{
            {"BaseElementModel", this->id()},
            {"BaseScenario", m_baseScenario->id()},
            {"BaseConstraintModel", {}}
        }
    };
    cmd2.redo();
    auto rack = *constraint_model.racks().begin();

    ShowRackInViewModel cmd3 {
        ObjectPath{
            {"BaseElementModel", this->id()},
            {"BaseScenario", m_baseScenario->id()},
            {"BaseConstraintModel", {}},
            {"FullViewConstraintViewModel", viewmodel->id()}
        },
        rack->id() };
    cmd3.redo();

    AddSlotToRack cmd4
    {
        ObjectPath{
            {"BaseElementModel", this->id()},
            {"BaseScenario", m_baseScenario->id()},
            {"BaseConstraintModel", {}},
            {"RackModel", rack->id() }
        }
    };
    cmd4.redo();
    auto slotId = (*rack->getSlots().begin())->id();

    ResizeSlotVertically cmd5
    {
        ObjectPath{
            {"BaseElementModel", this->id()},
            {"BaseScenario", m_baseScenario->id()},
            {"BaseConstraintModel", {}},
            {"RackModel", rack->id() },
            {"SlotModel", slotId}
        },
        1500
    };
    cmd5.redo();

    AddLayerModelToSlot cmd6
    {
        {
            {"BaseElementModel", this->id()},
            {"BaseScenario", m_baseScenario->id()},
            {"BaseConstraintModel", {}},
            {"RackModel", rack->id() },
            {"SlotModel", slotId}
        },
        {
            {"BaseElementModel", this->id()},
            {"BaseScenario", m_baseScenario->id()},
            {"BaseConstraintModel", {}},
            {"ScenarioModel", scenarioId}
        }
    };
    cmd6.redo();
}
ConstraintModel& BaseElementModel::baseConstraint() const
{
    return m_baseScenario->baseConstraint();
}

namespace {
void updateSlotFocus(const LayerModel* lm, bool b)
{
    if(lm && lm->parent())
    {
        if(auto slot = dynamic_cast<SlotModel*>(lm->parent()))
        {
            slot->setFocus(b);
        }
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
    // TODO why not presenter ?
    // Enable focus on the new viewmodel
    updateSlotFocus(process, true);
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
        m_focusManager.focusNothing();
    }
    else if(std::any_of(s.begin(),
                        s.end(),
                        [&] (const QObject* obj)
    {
        return obj == &displayedElements.displayedConstraint()
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

void BaseElementModel::setDisplayedConstraint(const ConstraintModel* constraint)
{
    displayedElements.setDisplayedConstraint(constraint);

    m_focusManager.focusNothing();

    disconnect(m_constraintConnection);
    if(constraint != &m_baseScenario->baseConstraint())
    {
        m_constraintConnection =
                connect(constraint->fullView(), &QObject::destroyed,
                        this, [&] () { setDisplayedConstraint(&m_baseScenario->baseConstraint()); });
    }

    emit displayedConstraintChanged();
}
