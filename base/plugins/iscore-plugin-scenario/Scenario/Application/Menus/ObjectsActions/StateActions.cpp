#include "StateActions.hpp"

#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Cohesion/RefreshStates.hpp>


#include <core/document/Document.hpp>

#include <Scenario/Application/ScenarioActions.hpp>

#include <Process/Layer/LayerContextMenu.hpp>
#include <QAction>
#include <QMenu>
#include <iscore/widgets/SetIcons.hpp>
namespace Scenario
{
StateActions::StateActions(ScenarioApplicationPlugin* parent) :
    m_parent{parent}
{
    m_refreshStates = new QAction {tr("Refresh states"), this};
    m_refreshStates->setShortcutContext(Qt::ApplicationShortcut);
    m_refreshStates->setShortcut(tr("Ctrl+U"));
    m_refreshStates->setToolTip(tr("Ctrl+U"));
    setIcons(m_refreshStates, QString(":/icons/refresh_on.png"), QString(":/icons/refresh_off.png"));

    connect(m_refreshStates, &QAction::triggered,
            this, [&] () {
        Command::RefreshStates(m_parent->currentDocument()->context());
    });

}


void StateActions::makeGUIElements(iscore::GUIElements& ref)
{
    using namespace iscore;

    Menu& object = m_parent->context.menus.get().at(Menus::Object());
    object.menu()->addAction(m_refreshStates);

    Toolbar& tb = *find_if(ref.toolbars, [] (auto& tb) {
        return tb.key() == StringKey<iscore::Toolbar>("Constraint");
    });
    tb.toolbar()->addAction(m_refreshStates);

    ref.actions.add<Actions::RefreshStates>(m_refreshStates);
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
            stateSubmenu->addAction(m_refreshStates);
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
