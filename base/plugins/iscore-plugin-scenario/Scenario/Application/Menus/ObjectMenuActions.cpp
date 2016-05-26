#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteContent.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElements.hpp>
#include <Scenario/Commands/State/InsertContentInState.hpp>
#include <Scenario/Commands/Constraint/InsertContentInConstraint.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioGlobalCommandManager.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <QAction>
#include <QByteArray>
#include <QClipboard>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QKeySequence>
#include <QMenu>
#include <qnamespace.h>
#include <QObject>

#include <QRect>
#include <QString>
#include <QToolBar>
#include <algorithm>

#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include "ObjectMenuActions.hpp"
#include <Process/LayerModel.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include "ScenarioCopy.hpp"
#include "TextDialog.hpp"

#include <iscore/application/ApplicationContext.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <Scenario/Commands/Cohesion/DoForSelectedConstraints.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <Scenario/Process/Algorithms/ContainersAccessors.hpp>
#include <Scenario/Application/ScenarioActions.hpp>

namespace Scenario
{
ObjectMenuActions::ObjectMenuActions(
        ScenarioApplicationPlugin* parent) :
    m_parent{parent},
    m_eventActions{parent},
    m_cstrActions{parent},
    m_stateActions{parent}
{
    using namespace iscore;

    // REMOVE
    m_removeElements = new QAction{tr("Remove selected elements"), this};
    m_removeElements->setShortcut(Qt::Key_Backspace); //NOTE : the effective shortcut is in CommonSelectionState.cpp
    m_removeElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_removeElements, &QAction::triggered,
            [this]()
    {
        if (auto sm = m_parent->focusedScenarioModel())
        {
            Scenario::removeSelection(*sm, m_parent->currentDocument()->context().commandStack);
        }
    });

    m_clearElements = new QAction{tr("Clear selected elements"), this};
    m_clearElements->setShortcut(QKeySequence::Delete); //NOTE : the effective shortcut is in CommonSelectionState.cpp
    m_clearElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_clearElements, &QAction::triggered,
            [this]()
    {
        if (auto sm = m_parent->focusedScenarioModel())
        {
            Scenario::clearContentFromSelection(*sm, m_parent->currentDocument()->context().commandStack);
        }
    });

    // COPY/CUT
    m_copyContent = new QAction{tr("Copy"), this};
    m_copyContent->setShortcut(QKeySequence::Copy);
    m_copyContent->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_copyContent, &QAction::triggered,
            [this]()
    {
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
        TextDialog s{doc.toJson(QJsonDocument::Indented), qApp->activeWindow()};

        s.exec();
    });

}

void ObjectMenuActions::makeGUIElements(iscore::GUIElements& ref)
{
    using namespace iscore;
    auto& actions = ref.actions;

    auto& scenariomodel_cond = m_parent->context.actions.condition<EnableWhenScenarioModelObject>();
    auto& scenarioiface_cond = m_parent->context.actions.condition<EnableWhenScenarioInterfaceObject>();
    actions.add<Actions::RemoveElements>(m_removeElements);
    actions.add<Actions::ClearElements>(m_clearElements);
    actions.add<Actions::CopyContent>(m_copyContent);
    actions.add<Actions::CutContent>(m_cutContent);
    actions.add<Actions::PasteContent>(m_pasteContent);
    actions.add<Actions::ElementsToJson>(m_elementsToJson);

    scenariomodel_cond.add<Actions::RemoveElements>();
    scenarioiface_cond.add<Actions::ClearElements>();
    scenarioiface_cond.add<Actions::CopyContent>();
    scenarioiface_cond.add<Actions::CutContent>();
    scenarioiface_cond.add<Actions::PasteContent>();
    scenarioiface_cond.add<Actions::ElementsToJson>();

    Menu& object = m_parent->context.menus.get().at(Menus::Object());
    object.menu()->addAction(m_elementsToJson);
    object.menu()->addAction(m_removeElements);
    object.menu()->addAction(m_clearElements);
    object.menu()->addSeparator();
    object.menu()->addAction(m_copyContent);
    object.menu()->addAction(m_cutContent);
    object.menu()->addAction(m_pasteContent);
    object.menu()->addSeparator();
    m_eventActions.makeGUIElements(ref);
    m_cstrActions.makeGUIElements(ref);
    m_stateActions.makeGUIElements(ref);
}

void ObjectMenuActions::setupContextMenu(Process::LayerContextMenuManager &ctxm)
{
    using namespace Process;
    LayerContextMenu scenario_model = MetaContextMenu<ContextMenus::ScenarioModelContextMenu>::make();
    LayerContextMenu scenario_object = MetaContextMenu<ContextMenus::ScenarioObjectContextMenu>::make();

    // Used for scenario model
    scenario_model.functions.push_back(
                [this] (QMenu& menu, QPoint, QPointF scenePoint, const LayerContext& ctx)
    {
        auto& scenario = *safe_cast<const TemporalScenarioPresenter*>(&ctx.layerPresenter);
        auto sel = ctx.context.selectionStack.currentSelection();
        if(!sel.empty())
        {
            auto objectMenu = menu.addMenu(tr("Object"));

            objectMenu->addAction(m_elementsToJson);
            objectMenu->addAction(m_removeElements);
            objectMenu->addAction(m_clearElements);
            objectMenu->addSeparator();

            objectMenu->addAction(m_copyContent);
            objectMenu->addAction(m_cutContent);
            objectMenu->addAction(m_pasteContent);
        }

        auto pasteElements = new QAction{tr("Paste element(s)"), this};
        pasteElements->setShortcut(QKeySequence::Paste);
        pasteElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(pasteElements, &QAction::triggered,
                [&,scenePoint]()
        {
            this->pasteElements(QJsonDocument::fromJson(QApplication::clipboard()->text().toUtf8()).object(),
                          Scenario::ConvertToScenarioPoint(
                                    scenePoint,
                                    scenario.zoomRatio(),
                                    scenario.view().boundingRect().height()));
        });
        menu.addAction(pasteElements);
    });

    // Used for base scenario, loops, etc.
    scenario_object.functions.push_back(
                [this] (QMenu& menu, QPoint, QPointF, const LayerContext& ctx)
    {
        auto sel = ctx.context.selectionStack.currentSelection();
        if(!sel.empty())
        {
            auto objectMenu = menu.addMenu(tr("Object"));

            objectMenu->addAction(m_elementsToJson);
            objectMenu->addAction(m_clearElements);
            objectMenu->addSeparator();

            objectMenu->addAction(m_copyContent);
            objectMenu->addAction(m_pasteContent);
        }
    });
    m_eventActions.setupContextMenu(ctxm);
    m_cstrActions.setupContextMenu(ctxm);
    m_stateActions.setupContextMenu(ctxm);

    ctxm.insert(std::move(scenario_model));
    ctxm.insert(std::move(scenario_object));
}

QJsonObject ObjectMenuActions::copySelectedElementsToJson()
{
    auto si = m_parent->focusedScenarioInterface();
    auto si_obj = dynamic_cast<QObject*>(const_cast<ScenarioInterface*>(si));
    if(auto sm = dynamic_cast<const Scenario::ScenarioModel*>(si))
    {
        return copySelectedScenarioElements(*sm);
    }
    else if(auto bsm = dynamic_cast<const Scenario::BaseScenarioContainer*>(si))
    {
        return copySelectedScenarioElements(*bsm, si_obj);
    }
    else
    {
        // Full-view copy
        auto& bem = iscore::IDocument::modelDelegate<Scenario::ScenarioDocumentModel>(*m_parent->currentDocument());
        if(!bem.baseScenario().selectedChildren().empty())
        {
            return copySelectedScenarioElements(bem.baseScenario(), &bem.baseScenario());
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
        Scenario::clearContentFromSelection(*sm, m_parent->currentDocument()->context().commandStack);
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
    auto cmd = new Command::ScenarioPasteElements(sm, obj, origin);

    dispatcher().submitCommand(cmd);
}

template<typename Scenario_T>
static void writeJsonToScenario(
        const Scenario_T& scen,
        const ObjectMenuActions& self,
        const QJsonObject& obj)
{
    MacroCommandDispatcher dispatcher{new Command::ScenarioPasteContent, self.dispatcher().stack()};
    auto selectedConstraints = selectedElements(getConstraints(scen));
    auto expandMode = self.appPlugin()->editionSettings().expandMode();
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

    auto selectedStates = selectedElements(getStates(scen));
    for(const auto& json_vref : obj["States"].toArray())
    {
        for(const auto& state : selectedStates)
        {
            auto cmd = new Command::InsertContentInState{
                       json_vref.toObject(),
                       *state};

            dispatcher.submitCommand(cmd);
        }
    }

    dispatcher.commit();
}

void ObjectMenuActions::writeJsonToSelectedElements(const QJsonObject &obj)
{
    auto si = m_parent->focusedScenarioInterface();
    if(auto sm = dynamic_cast<const Scenario::ScenarioModel*>(si))
    {
        writeJsonToScenario(*sm, *this, obj);
    }
    else if(auto bsm = dynamic_cast<const Scenario::BaseScenarioContainer*>(si))
    {
        writeJsonToScenario(*bsm, *this, obj);
    }
    else
    {
        ISCORE_TODO;
        /*
        // Full-view paste
        auto& bem = iscore::IDocument::modelDelegate<ScenarioDocumentModel>(*m_parent->currentDocument());
        if(bem.baseConstraint().selection.get())
        {
            return copySelectedScenarioElements(bem.baseScenario());
        }*/
    }

}

CommandDispatcher<> ObjectMenuActions::dispatcher() const
{
    CommandDispatcher<> disp{m_parent->currentDocument()->context().commandStack};
    return disp;
}


}
