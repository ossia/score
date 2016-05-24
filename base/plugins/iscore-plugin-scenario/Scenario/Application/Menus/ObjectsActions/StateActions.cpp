#include "StateActions.hpp"

#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Cohesion/RefreshStates.hpp>

#include <core/presenter/MenubarManager.hpp>
#include <core/document/Document.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
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
    m_updateStates->setWhatsThis(iscore::MenuInterface::name(iscore::ContextMenu::State));
    connect(m_updateStates, &QAction::triggered,
            this, [&] () {
        Command::RefreshStates(m_parent->currentDocument()->context());
    });

}


void StateActions::makeGUIElements(iscore::GUIElements& ref)
{
    using namespace iscore;
    auto& scenario_iface_cond = m_parent->context.actions.condition<Process::EnableWhenFocusedProcessIs<Scenario::ScenarioInterface>>();

    Menu& object = m_parent->context.menus.get().at(Menus::Object());
    object.menu()->addAction(m_updateStates);

    ref.actions.add<Actions::RefreshStates>(m_updateStates);
    scenario_iface_cond.add<Actions::RefreshStates>();
}

void StateActions::fillContextMenu(
        QMenu* menu,
        const Selection& sel,
        const TemporalScenarioPresenter& pres,
        const QPoint&,
        const QPointF&)
{
    using namespace iscore;
    if(!sel.empty())
    {
        if(std::any_of(sel.cbegin(),
                       sel.cend(),
                       [] (const QObject* obj) { return dynamic_cast<const StateModel*>(obj); })) // TODO : event or timenode ?
        {
            auto stateSubmenu = menu->findChild<QMenu*>(MenuInterface::name(iscore::ContextMenu::State));
            if(!stateSubmenu)
            {
                stateSubmenu = menu->addMenu(MenuInterface::name(iscore::ContextMenu::State));
                stateSubmenu->setTitle(MenuInterface::name(iscore::ContextMenu::State));
            }

            stateSubmenu->addAction(m_updateStates);
        }
    }
}

CommandDispatcher<> StateActions::dispatcher()
{
    CommandDispatcher<> disp{m_parent->currentDocument()->context().commandStack};
    return disp;
}
}
