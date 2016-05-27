#include "StateActions.hpp"

#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Cohesion/RefreshStates.hpp>

#include <core/presenter/MenubarManager.hpp>
#include <core/document/Document.hpp>

#include <Scenario/Application/ScenarioActions.hpp>

#include <Process/Layer/LayerContextMenu.hpp>
#include <QAction>
#include <QMenu>

namespace Scenario
{
StateActions::StateActions(ScenarioApplicationPlugin* parent) :
    m_parent{parent}
{
    m_updateStates = new QAction {tr("Refresh states"), this};
    m_updateStates->setShortcutContext(Qt::ApplicationShortcut);
    m_updateStates->setShortcut(tr("Ctrl+U"));
    m_updateStates->setToolTip(tr("Ctrl+U"));

    connect(m_updateStates, &QAction::triggered,
            this, [&] () {
        Command::RefreshStates(m_parent->currentDocument()->context());
    });

}


void StateActions::makeGUIElements(iscore::GUIElements& ref)
{
    using namespace iscore;

    Menu& object = m_parent->context.menus.get().at(Menus::Object());
    object.menu()->addAction(m_updateStates);

    ref.actions.add<Actions::RefreshStates>(m_updateStates);
    auto& cond = m_parent->context.actions.condition<iscore::EnableWhenSelectionContains<Scenario::StateModel>>();
    cond.add<Actions::RefreshStates>();
}

void StateActions::setupContextMenu(Process::LayerContextMenuManager& ctxm)
{
    using namespace Process;
    Process::LayerContextMenu cm = MetaContextMenu<ContextMenus::StateContextMenu>::make();

    cm.functions.push_back(
    [this] (QMenu& menu, QPoint, QPointF, const Process::LayerContext& ctx)
    {
        using namespace iscore;
        auto sel = ctx.context.selectionStack.currentSelection();
        if(sel.empty())
            return;

        if(any_of(sel, matches<Scenario::StateModel>{})) // TODO : event or timenode ?
        {
            auto stateSubmenu = menu.addMenu(tr("State"));
            stateSubmenu->setObjectName("State");
            stateSubmenu->addAction(m_updateStates);
        }
    });

    ctxm.insert(std::move(cm));
}

CommandDispatcher<> StateActions::dispatcher()
{
    CommandDispatcher<> disp{m_parent->currentDocument()->context().commandStack};
    return disp;
}
}
