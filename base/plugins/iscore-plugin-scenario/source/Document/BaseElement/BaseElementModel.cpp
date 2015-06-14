#include "BaseElementModel.hpp"

#include "source/Document/Constraint/ConstraintModel.hpp"
#include "source/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"

#include <iscore/document/DocumentInterface.hpp>

#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Commands/Constraint/AddBoxToConstraint.hpp"
#include "Commands/Constraint/Box/AddDeckToBox.hpp"
#include "Commands/Constraint/Box/Deck/ResizeDeckVertically.hpp"
#include "Commands/Constraint/Box/Deck/AddProcessViewModelToDeck.hpp"
#include "Commands/Scenario/ShowBoxInViewModel.hpp"

#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/ProcessViewModel.hpp"

#include <QApplication>
using namespace Scenario;

#include <core/document/Document.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include "Control/ScenarioControl.hpp"
#include <iscore/presenter/PresenterInterface.hpp>

BaseElementModel::BaseElementModel(QObject* parent) :
    iscore::DocumentDelegateModelInterface {id_type<iscore::DocumentDelegateModelInterface>(getNextId()), "BaseElementModel", parent},
    m_baseConstraint {new ConstraintModel{
                            id_type<ConstraintModel>{0},
                            id_type<AbstractConstraintViewModel>{0},
                            0,
                            this}}
{
    ConstraintModel::Algorithms::changeAllDurations(*m_baseConstraint, std::chrono::minutes{3});
    m_baseConstraint->setObjectName("BaseConstraintModel");

    initializeNewDocument(m_baseConstraint->fullView());

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
            {"BaseConstraintModel", {}}
        },
        "Scenario"
    };
    cmd1.redo();
    auto scenarioId = constraint_model.processes().front()->id();

    AddBoxToConstraint cmd2
    {
        ObjectPath{
            {"BaseElementModel", this->id()},
            {"BaseConstraintModel", {}}
        }
    };
    cmd2.redo();
    auto box = constraint_model.boxes().front();

    ShowBoxInViewModel cmd3 {
        ObjectPath{
            {"BaseElementModel", this->id()},
            {"BaseConstraintModel", {}},
            {"FullViewConstraintViewModel", viewmodel->id()}
        },
        box->id() };
    cmd3.redo();

    AddDeckToBox cmd4
    {
        ObjectPath{
            {"BaseElementModel", this->id()},
            {"BaseConstraintModel", {}},
            {"BoxModel", box->id() }
        }
    };
    cmd4.redo();
    auto deckId = box->decks().front()->id();

    ResizeDeckVertically cmd5
    {
        ObjectPath{
            {"BaseElementModel", this->id()},
            {"BaseConstraintModel", {}},
            {"BoxModel", box->id() },
            {"DeckModel", deckId}
        },
        1500
    };
    cmd5.redo();

    AddProcessViewModelToDeck cmd6
    {
        {
            {"BaseElementModel", this->id()},
            {"BaseConstraintModel", {}},
            {"BoxModel", box->id() },
            {"DeckModel", deckId}
        },
        {
            {"BaseElementModel", this->id()},
            {"BaseConstraintModel", {}},
            {"ScenarioModel", scenarioId}
        }
    };
    cmd6.redo();
}

namespace {
void updateDeckFocus(const ProcessViewModel* pvm, bool b)
{
    if(pvm && pvm->parent())
    {
        if(auto deck = dynamic_cast<DeckModel*>(pvm->parent()))
        {
            deck->setFocus(b);
        }
    }
}
}

void BaseElementModel::on_viewModelDefocused(const ProcessViewModel* vm)
{
    // Disable the focus on previously focused view model
    updateDeckFocus(vm, false);

    // Deselect
    iscore::SelectionDispatcher selectionDispatcher(
                iscore::IDocument::documentFromObject(*this)->selectionStack());
    selectionDispatcher.setAndCommit({});
}

void BaseElementModel::on_viewModelFocused(const ProcessViewModel* process)
{
    // TODO why not presenter ?
    // Enable focus on the new viewmodel
    updateDeckFocus(process, true);
}

// TODO candidate for ProcessSelectionManager.
void BaseElementModel::setNewSelection(const Selection& s)
{;
    auto process = m_focusManager.focusedModel();

    if(s.empty())
    {
        if(process)
        {
            process->setSelection({});
            m_displayedConstraint->selection.set(false);
            m_focusManager.focusNothing();
        }
    }
    else if(*s.begin() == m_displayedConstraint)
    {
        if(process)
        {
            process->setSelection({});
            m_focusManager.focusNothing();
        }

        m_displayedConstraint->selection.set(true);
    }
    else
    {
        // We know by the presenter that all objects
        // in a given selection are in the same Process.
        m_displayedConstraint->selection.set(false);
        auto newProc = parentProcess(*s.begin());
        if(process && newProc != process)
        {
            process->setSelection({});
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
    // TODO only keep it saved at one place.
    m_displayedConstraint = constraint;
    m_focusManager.focusNothing();
}
