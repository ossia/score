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
#include <boost/optional/optional.hpp>
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

#include <Scenario/Application/Menus/ObjectsActions/EventActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/ConstraintActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/StateActions.hpp>
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

namespace Scenario
{
ObjectMenuActions::ObjectMenuActions(
        iscore::ToplevelMenuElement menuElt,
        ScenarioApplicationPlugin* parent) :
    ScenarioActions(menuElt, parent)
{
    using namespace iscore;
    m_eventActions = new EventActions{menuElt, parent};
    m_cstrActions = new ConstraintActions{menuElt, parent};
    m_stateActions = new StateActions{menuElt, parent};

    // REMOVE
    m_removeElements = new QAction{tr("Remove selected elements"), this};
    m_removeElements->setShortcut(Qt::Key_Backspace); //NOTE : the effective shortcut is in CommonSelectionState.cpp
    m_removeElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_removeElements->setWhatsThis(MenuInterface::name(iscore::ContextMenu::Object));
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
    m_clearElements->setWhatsThis(MenuInterface::name(iscore::ContextMenu::Object));
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
    m_copyContent->setWhatsThis(MenuInterface::name(iscore::ContextMenu::Object));
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
    m_cutContent->setWhatsThis(MenuInterface::name(iscore::ContextMenu::Object));
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
    m_pasteContent->setWhatsThis(MenuInterface::name(iscore::ContextMenu::Object));
    connect(m_pasteContent, &QAction::triggered,
            [this]()
    {
        writeJsonToSelectedElements(
                    QJsonDocument::fromJson(
                        QApplication::clipboard()->text().toUtf8()).object());
    });

    // DISPLAY JSON
    m_elementsToJson = new QAction{tr("Convert selection to JSON"), this};
    m_elementsToJson->setWhatsThis(MenuInterface::name(iscore::ContextMenu::Object));
    connect(m_elementsToJson, &QAction::triggered,
            [this]()
    {
        QJsonDocument doc{copySelectedElementsToJson()};
        TextDialog s{doc.toJson(QJsonDocument::Indented), qApp->activeWindow()};

        s.exec();
    });

}

void ObjectMenuActions::fillMenuBar(iscore::MenubarManager* menu)
{
    menu->insertActionIntoToplevelMenu(m_menuElt, m_elementsToJson);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_removeElements);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_clearElements);
    menu->addSeparatorIntoToplevelMenu(m_menuElt, iscore::EditMenuElement::Separator_Copy);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_copyContent);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_cutContent);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_pasteContent);

    m_eventActions->fillMenuBar(menu);
    m_cstrActions->fillMenuBar(menu);
    m_stateActions->fillMenuBar(menu);
}

void ObjectMenuActions::fillContextMenu(
        QMenu *menu,
        const Selection& sel,
        const TemporalScenarioPresenter& pres,
        const QPoint& p,
        const QPointF& scenePoint)
{
    using namespace iscore;

    m_eventActions->fillContextMenu(menu, sel, pres, p, scenePoint );
    m_cstrActions->fillContextMenu(menu, sel, pres, p, scenePoint );
    m_stateActions->fillContextMenu(menu, sel, pres, p ,scenePoint);

    if(!sel.empty())
    {
        auto objectMenu = menu->findChild<QMenu*>(MenuInterface::name(iscore::ContextMenu::Object));
        if(!objectMenu)
        {
            objectMenu = menu->addMenu(MenuInterface::name(iscore::ContextMenu::Object));
            objectMenu->setTitle(MenuInterface::name(iscore::ContextMenu::Object));
        }

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
                      Scenario::ConvertToScenarioPoint(scenePoint, pres.zoomRatio(), pres.view().boundingRect().height()));
    });
    menu->addAction(pasteElements);
}

void ObjectMenuActions::setEnabled(bool b)
{
    for (auto& act : actions())
    {
        act->setEnabled(b);
    }
    m_eventActions->setEnabled(b);
    m_cstrActions->setEnabled(b);
    m_stateActions->setEnabled(b);
}

bool ObjectMenuActions::populateToolBar(QToolBar* tb)
{
    return m_cstrActions->populateToolBar(tb);
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
        auto& bem = iscore::IDocument::modelDelegate<ScenarioDocumentModel>(*m_parent->currentDocument());
        if(bem.baseConstraint().selection.get())
        {
            return copySelectedScenarioElements(bem.baseScenario());
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

CommandDispatcher<> ObjectMenuActions::dispatcher()
{
    CommandDispatcher<> disp{m_parent->currentDocument()->context().commandStack};
    return disp;
}


QList<QAction*> ObjectMenuActions::actions() const
{
    QList<QAction*> lst{
            m_removeElements,
            m_clearElements,
            m_copyContent,
            m_cutContent,
            m_pasteContent,
            m_elementsToJson,
        };
    lst.append(m_eventActions->actions());
    lst.append(m_cstrActions->actions());
    lst.append(m_stateActions->actions());
    return lst;
}
}
