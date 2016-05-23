#include "StateActions.hpp"

#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Cohesion/RefreshStates.hpp>

#include <core/presenter/MenubarManager.hpp>
#include <core/document/Document.hpp>

#include <QAction>
#include <QMenu>

namespace Scenario
{
StateActions::StateActions(iscore::ToplevelMenuElement menuElt,
               ScenarioApplicationPlugin* parent) :
    m_menuElt{menuElt},
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


void StateActions::fillMenuBar(iscore::MenubarManager* menu)
{
    menu->insertActionIntoToplevelMenu(m_menuElt,
                       m_updateStates);
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

void StateActions::setEnabled(bool b)
{
    for (auto& act : actions())
    {
    act->setEnabled(b);
    }
}

QList<QAction*> StateActions::actions() const
{
    return {m_updateStates};
}

CommandDispatcher<> StateActions::dispatcher()
{
    CommandDispatcher<> disp{m_parent->currentDocument()->context().commandStack};
    return disp;
}
}
