#include "ObjectMenuActions.hpp"

#include <core/document/DocumentModel.hpp>

#include <iscore/menu/MenuInterface.hpp>

#include <Scenario/Process/ScenarioGlobalCommandManager.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>

#include <Scenario/Document/BaseElement/BaseElementModel.hpp>
#include <Scenario/Document/BaseElement/BaseElementPresenter.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>

#include <Scenario/Commands/Constraint/ReplaceConstraintContent.hpp>
#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>
#include <Scenario/Commands/TimeNode/AddTrigger.hpp>
#include <Scenario/Commands/TimeNode/RemoveTrigger.hpp>

#include <Scenario/Control/ScenarioControl.hpp>
#include <Scenario/Commands/Cohesion/InterpolateStates.hpp>
#include <Scenario/Commands/Cohesion/UpdateStates.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteContent.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElements.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>
#include <Scenario/Commands/Scenario/HideRackInViewModel.hpp>
#include <Scenario/Commands/State/InsertContentInState.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>

#include "ScenarioCopy.hpp"
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>

#include <QJsonDocument>
#include <QApplication>
#include <QClipboard>

#include <QTextEdit>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QTextBlock>
#include <QDialog>
#include "TextDialog.hpp"

ObjectMenuActions::ObjectMenuActions(
        iscore::ToplevelMenuElement menuElt,
        ScenarioControl* parent) :
    ScenarioActions(menuElt, parent)
{
    // REMOVE
    m_removeElements = new QAction{tr("Remove selected elements"), this};
    m_removeElements->setShortcut(Qt::Key_Backspace);
    m_removeElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_removeElements, &QAction::triggered,
            [this]()
    {
        if (auto sm = m_parent->focusedScenarioModel())
        {
            ScenarioGlobalCommandManager mgr{m_parent->currentDocument()->commandStack()};
            mgr.removeSelection(*sm);
        }
    });

    m_clearElements = new QAction{tr("Clear selected elements"), this};
    m_clearElements->setShortcut(QKeySequence::Delete);
    m_clearElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_clearElements, &QAction::triggered,
            [this]()
    {
        if (auto sm = m_parent->focusedScenarioModel())
        {
            ScenarioGlobalCommandManager mgr{m_parent->currentDocument()->commandStack()};
            mgr.clearContentFromSelection(*sm);
        }
    });

    // COPY/CUT
    m_copyContent = new QAction{tr("Copy"), this};
    m_copyContent->setShortcut(QKeySequence::Copy);
    m_copyContent->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_copyContent, &QAction::triggered,
            [this]()
    {
        qDebug("da");
        auto obj = copySelectedElementsToJson();
        if(obj.empty())
            return;
        QJsonDocument doc{obj};
        auto clippy = QApplication::clipboard();
        clippy->setText(doc.toJson(QJsonDocument::Indented));
    });

    m_cutContent = new QAction{tr("Cut"), this};
    m_cutContent->setShortcut(QKeySequence::Cut);
    m_cutContent->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_cutContent, &QAction::triggered,
            [this]()
    {
        auto obj = cutSelectedElementsToJson();
        if(obj.empty())
            return;
        QJsonDocument doc{obj};
        auto clippy = QApplication::clipboard();
        clippy->setText(doc.toJson(QJsonDocument::Indented));
    });

    m_pasteContent = new QAction{tr("Paste content"), this};
    m_pasteContent->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_pasteContent, &QAction::triggered,
            [this]()
    {
        writeJsonToSelectedElements(
                    QJsonDocument::fromJson(
                        QApplication::clipboard()->text().toUtf8()).object());
    });

    // DISPLAY JSON
    m_elementsToJson = new QAction{tr("Convert selection to JSON"), this};
    connect(m_elementsToJson, &QAction::triggered,
            [this]()
    {
        QJsonDocument doc{copySelectedElementsToJson()};
        auto s = new TextDialog{doc.toJson(QJsonDocument::Indented), qApp->activeWindow()};

        s->show();
    });

    // ADD PROCESS
    m_addProcessDialog = new AddProcessDialog(qApp->activeWindow());

    connect(m_addProcessDialog, &AddProcessDialog::okPressed,
            this, &ObjectMenuActions::addProcessInConstraint);

    m_addProcess = new QAction{tr("Add Process in constraint"), this};
    connect(m_addProcess, &QAction::triggered,
            [this]()
    {
        auto selectedConstraints = selectedElements(m_parent->focusedScenarioModel()->constraints);
        if(selectedConstraints.isEmpty())
            return;
        m_addProcessDialog->launchWindow();
    });


    m_interp = new QAction {tr("Interpolate states"), this};
    m_interp->setShortcutContext(Qt::ApplicationShortcut);
    m_interp->setShortcut(tr("Ctrl+K"));
    m_interp->setToolTip(tr("Ctrl+K"));
    connect(m_interp, &QAction::triggered,
            this, [&] () {
        InterpolateStates(m_parent->currentDocument());
    });


    m_updateStates = new QAction {tr("Refresh states"), this};
    m_updateStates->setShortcutContext(Qt::ApplicationShortcut);
    m_updateStates->setShortcut(tr("Ctrl+U"));
    m_updateStates->setToolTip(tr("Ctrl+U"));
    connect(m_updateStates, &QAction::triggered,
            this, [&] () {
        RefreshStates(m_parent->currentDocument());
    });


    // ADD TRIGGER
    m_addTrigger = new QAction{tr("Add Trigger"), this};
    connect(m_addTrigger, &QAction::triggered,
            this, &ObjectMenuActions::addTriggerToTimeNode);

    m_removeTrigger = new QAction{tr("Remove Trigger"), this};
    connect(m_removeTrigger, &QAction::triggered,
            this, &ObjectMenuActions::removeTriggerFromTimeNode);

}

void ObjectMenuActions::fillMenuBar(iscore::MenubarManager* menu)
{
    menu->insertActionIntoToplevelMenu(m_menuElt, m_addProcess);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_addTrigger);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_removeTrigger);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_elementsToJson);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_removeElements);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_clearElements);
    menu->addSeparatorIntoToplevelMenu(m_menuElt, iscore::EditMenuElement::Separator_Copy);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_copyContent);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_cutContent);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_pasteContent);

    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::ObjectMenu,
                                       m_interp);
    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::ObjectMenu,
                                       m_updateStates);
}

void ObjectMenuActions::fillContextMenu(
        QMenu *menu,
        const Selection& sel,
        const TemporalScenarioPresenter& pres,
        const QPoint&,
        const QPointF& scenePoint)
{
    if(!sel.empty())
    {
        QList<const ConstraintModel*> selectedConstraints = filterSelectionByType<ConstraintModel>(sel);
        if(selectedConstraints.size() == 1)
        {
            auto rackMenu = menu->addMenu(tr("Rack"));
            auto& cst = *selectedConstraints.front();

            // We have to find the constraint view model of this layer.
            auto& vm = dynamic_cast<const TemporalScenarioLayerModel*>(&pres.layerModel())->constraint(cst.id());

            for(const RackModel& rack : cst.racks)
            {
                auto act = new QAction{rack.objectName(), rackMenu};
                connect(act, &QAction::triggered,
                        this, [&] () {
                    auto cmd = new Scenario::Command::ShowRackInViewModel{vm, rack.id()};
                    CommandDispatcher<> dispatcher{m_parent->currentDocument()->commandStack()};
                    dispatcher.submitCommand(cmd);
                });

                rackMenu->addAction(act);
            }

            auto hideAct = new QAction{tr("Hide"), rackMenu};
            connect(hideAct, &QAction::triggered,
                    this, [&] () {
                auto cmd = new Scenario::Command::HideRackInViewModel{vm};
                CommandDispatcher<> dispatcher{m_parent->currentDocument()->commandStack()};
                dispatcher.submitCommand(cmd);
            });
            rackMenu->addAction(hideAct);
        }

        if(selectedConstraints.size() >= 1)
        {
            menu->addAction(m_addProcess);
            menu->addAction(m_interp);
            menu->addSeparator();
        }


        if(std::any_of(sel.cbegin(),
                       sel.cend(),
                       [] (const QObject* obj) { return dynamic_cast<const EventModel*>(obj); })) // TODO : event or timenode ?
        {
            menu->addAction(m_addTrigger);
            menu->addAction(m_removeTrigger);
            menu->addSeparator();
        }

        if(std::any_of(sel.cbegin(),
                       sel.cend(),
                       [] (const QObject* obj) { return dynamic_cast<const StateModel*>(obj); })) // TODO : event or timenode ?
        {
            menu->addAction(m_updateStates);
            menu->addSeparator();
        }

        menu->addAction(m_elementsToJson);
        menu->addAction(m_removeElements);
        menu->addAction(m_clearElements);
        menu->addSeparator();

        menu->addAction(m_copyContent);
        menu->addAction(m_cutContent);
        menu->addAction(m_pasteContent);
    }

    auto pasteElements = new QAction{tr("Paste here"), this};
    pasteElements->setShortcut(QKeySequence::Paste);
    pasteElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(pasteElements, &QAction::triggered,
            [&,scenePoint]()
    {
        this->pasteElements(QJsonDocument::fromJson(QApplication::clipboard()->text().toUtf8()).object(),
                      Scenario::ConvertToScenarioPoint(scenePoint, pres.zoomRatio(), pres.view().boundingRect().height()));
    });
    menu->addAction(pasteElements);

}

bool ObjectMenuActions::populateToolBar(QToolBar * b)
{
    b->addAction(m_interp);
    return true;
}

void ObjectMenuActions::setEnabled(bool b)
{
    for (auto& act : actions())
    {
        act->setEnabled(b);
    }
}

QJsonObject ObjectMenuActions::copySelectedElementsToJson()
{
    if (auto sm = m_parent->focusedScenarioModel())
    {
        return copySelectedScenarioElements(*sm);
    }
    else
    {
        // Full-view copy
        auto& bem = iscore::IDocument::modelDelegate<BaseElementModel>(*m_parent->currentDocument());
        if(bem.baseConstraint().selection.get())
        {
            return copyBaseConstraint(bem.baseConstraint());
        }
    }

    return {};
}

QJsonObject ObjectMenuActions::cutSelectedElementsToJson()
{
    auto obj = copySelectedElementsToJson();
    if(obj.empty())
        return {};

    if (auto sm = m_parent->focusedScenarioModel())
    {
        ScenarioGlobalCommandManager mgr{m_parent->currentDocument()->commandStack()};
        mgr.clearContentFromSelection(*sm);
    }

    return obj;
}

void ObjectMenuActions::pasteElements(
        const QJsonObject& obj,
        const Scenario::Point& origin)
{
    // TODO check for unnecessary uses of focusedProcessModel after focusedPresenter.
    auto pres = m_parent->focusedPresenter();
    if(!pres)
        return;

    auto& sm = static_cast<const TemporalScenarioLayerModel&>(pres->layerModel());
    // TODO check json validity
    auto cmd = new ScenarioPasteElements(sm, obj, origin);

    dispatcher().submitCommand(cmd);
}

void ObjectMenuActions::writeJsonToSelectedElements(const QJsonObject &obj)
{
    auto pres = m_parent->focusedPresenter();
    if(!pres)
        return;

    auto sm = m_parent->focusedScenarioModel();

    MacroCommandDispatcher dispatcher{new ScenarioPasteContent, this->dispatcher().stack()};
    auto selectedConstraints = selectedElements(sm->constraints);
    auto expandMode = pres->editionSettings().expandMode();
    for(const auto& json_vref : obj["Constraints"].toArray())
    {
        for(const auto& constraint : selectedConstraints)
        {
            auto cmd = new Scenario::Command::InsertContentInConstraint{
                       json_vref.toObject(),
                       *constraint,
                       expandMode};

            dispatcher.submitCommand(cmd);
        }
    }

    auto selectedStates = selectedElements(sm->states);
    for(const auto& json_vref : obj["States"].toArray())
    {
        for(const auto& state : selectedStates)
        {
            auto cmd = new InsertContentInState{
                       json_vref.toObject(),
                       *state};

            dispatcher.submitCommand(cmd);
        }
    }

    dispatcher.commit();
}

void ObjectMenuActions::addProcessInConstraint(const ProcessFactoryKey& processName)
{
    auto selectedConstraints = selectedElements(m_parent->focusedScenarioModel()->constraints);
    if(selectedConstraints.isEmpty())
        return;
    auto cmd = new Scenario::Command::AddProcessToConstraint //NOTE just the first, not all ?
    {
        **selectedConstraints.begin(),
        processName
    };
    emit dispatcher().submitCommand(cmd);
}

void ObjectMenuActions::addTriggerToTimeNode()
{
    auto selectedTimeNodes = selectedElements(m_parent->focusedScenarioModel()->timeNodes);// TODO : event or timenode ?
    if(selectedTimeNodes.isEmpty())
        return;

    auto cmd = new Scenario::Command::AddTrigger{**selectedTimeNodes.begin()};
    emit dispatcher().submitCommand(cmd);
}

void ObjectMenuActions::removeTriggerFromTimeNode()
{
    auto selectedTimeNodes = selectedElements(m_parent->focusedScenarioModel()->timeNodes);// TODO : event or timenode ?
    if(selectedTimeNodes.isEmpty())
        return;

    auto cmd = new Scenario::Command::RemoveTrigger{**selectedTimeNodes.begin()};
    emit dispatcher().submitCommand(cmd);
}

CommandDispatcher<> ObjectMenuActions::dispatcher()
{
    CommandDispatcher<> disp{m_parent->currentDocument()->commandStack()};
    return disp;
}


QList<QAction*> ObjectMenuActions::actions() const
{
    return {
            m_removeElements,
            m_clearElements,
            m_copyContent,
            m_cutContent,
            m_pasteContent,
            m_elementsToJson,
            m_addProcess,
            m_addTrigger
        };
}

