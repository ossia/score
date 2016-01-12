#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>
#include <Scenario/Commands/Constraint/AddOnlyProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/AddRackToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/ResizeSlotVertically.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Process/ScenarioProcessMetadata.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QList>
#include <QObject>

#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include "ScenarioDocumentModel.hpp"

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>


#include <Scenario/Document/DisplayedElements/DisplayedElementsProviderList.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <Process/LayerPresenter.hpp>
#include <core/document/Document.hpp>
#include <QFileInfo>
#include <algorithm>
#include <chrono>

namespace Process { class LayerPresenter; }
namespace Scenario
{
ScenarioDocumentModel::ScenarioDocumentModel(QObject* parent) :
    iscore::DocumentDelegateModelInterface {
        Id<iscore::DocumentDelegateModelInterface>(iscore::id_generator::getFirstId()),
        "ScenarioDocumentModel",
        parent},
    m_baseScenario{new BaseScenario{Id<BaseScenario>{0}, this}}
{
    m_baseScenario->constraint().duration.setRigid(false);
    ConstraintDurations::Algorithms::changeAllDurations(m_baseScenario->constraint(), std::chrono::minutes{3});
    m_baseScenario->endEvent().setDate(m_baseScenario->constraint().duration.defaultDuration());
    m_baseScenario->endTimeNode().setDate(m_baseScenario->constraint().duration.defaultDuration());
    m_baseScenario->constraint().setObjectName("BaseConstraintModel");

    auto& doc_metadata = iscore::IDocument::documentContext(*parent).document.metadata;
    m_baseScenario->constraint().metadata.setName(doc_metadata.fileName());

    connect(&doc_metadata, &iscore::DocumentMetadata::fileNameChanged,
            this, [&] (const QString& newName) {
        QFileInfo info(newName);

        m_baseScenario->constraint().metadata.setName(info.baseName());
    });

    initializeNewDocument(m_baseScenario->constraint().fullView());
    init();
}

void ScenarioDocumentModel::init()
{
    // Help for the FocusDispatcher.
    connect(this, &ScenarioDocumentModel::setFocusedPresenter,
            &m_focusManager, static_cast<void (Process::ProcessFocusManager::*)(QPointer<Process::LayerPresenter>)>(&Process::ProcessFocusManager::focus));

    con(m_focusManager, &Process::ProcessFocusManager::sig_defocusedViewModel,
            this, &ScenarioDocumentModel::on_viewModelDefocused);
    con(m_focusManager, &Process::ProcessFocusManager::sig_focusedViewModel,
            this, &ScenarioDocumentModel::on_viewModelFocused);
}

void ScenarioDocumentModel::initializeNewDocument(const FullViewConstraintViewModel *viewmodel)
{
    using namespace Scenario::Command;
    const auto& constraint_model = viewmodel->model();

    AddOnlyProcessToConstraint cmd1{
        iscore::IDocument::path(m_baseScenario->constraint()),
        ScenarioProcessMetadata::factoryKey()
    };
    cmd1.redo();

    AddRackToConstraint cmd2 {
        iscore::IDocument::path(m_baseScenario->constraint())
    };
    cmd2.redo();
    auto& rack = *constraint_model.racks.begin();

    ShowRackInViewModel cmd3 {
        iscore::IDocument::path(static_cast<const ConstraintViewModel&>(*viewmodel)),
        rack.id() };
    cmd3.redo();

    AddSlotToRack cmd4 {
        iscore::IDocument::path(*m_baseScenario->constraint().racks.begin()),
    };
    cmd4.redo();

    ResizeSlotVertically cmd5
    {
        iscore::IDocument::path(*m_baseScenario->constraint().racks.begin()->slotmodels.begin()),
        1500
    };
    cmd5.redo();

    AddLayerModelToSlot cmd6
    {
        iscore::IDocument::path(*m_baseScenario->constraint().racks.begin()->slotmodels.begin()),
        iscore::IDocument::path(*m_baseScenario->constraint().processes.begin())
    };
    cmd6.redo();
}

ConstraintModel& ScenarioDocumentModel::baseConstraint() const
{
    return m_baseScenario->constraint();
}

static void updateSlotFocus(const Process::LayerModel* lm, bool b)
{
    if(lm && lm->parent())
    {
        if(auto slot = dynamic_cast<SlotModel*>(lm->parent()))
        {
            slot->setFocus(b);
        }
    }
}

void ScenarioDocumentModel::on_viewModelDefocused(const Process::LayerModel* vm)
{
    // Disable the focus on previously focused view model
    updateSlotFocus(vm, false);

    // Deselect
    vm->processModel().setSelection({});
    iscore::IDocument::documentContext(*this).selectionStack.clear();
}

void ScenarioDocumentModel::on_viewModelFocused(const Process::LayerModel* process)
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
void ScenarioDocumentModel::setNewSelection(const Selection& s)
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
        return obj == &displayedElements.constraint()
            || obj == &displayedElements.startTimeNode()
            || obj == &displayedElements.endTimeNode()
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
        auto newProc = Process::parentProcess(*s.begin());
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

void ScenarioDocumentModel::setDisplayedConstraint(const ConstraintModel& constraint)
{
    if(displayedElements.initialized())
    {
        if(&constraint == &displayedElements.constraint())
            return;
    }

    auto& provider = iscore::IDocument::documentContext(*this).app.components.factory<DisplayedElementsProviderList>();
    displayedElements.setDisplayedElements(provider.make(constraint));

    m_focusManager.focusNothing();

    disconnect(m_constraintConnection);
    if(&constraint != &m_baseScenario->constraint())
    {
        m_constraintConnection =
                connect(constraint.fullView(), &QObject::destroyed,
                        this, [&] () { setDisplayedConstraint(m_baseScenario->constraint()); });
    }

    emit displayedConstraintChanged();
}
}
