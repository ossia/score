#pragma once

#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include <QAction>
#include <iscore/actions/Action.hpp>
#include <iscore_plugin_scenario_export.h>
namespace Scenario
{
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
class ISCORE_PLUGIN_SCENARIO_EXPORT StateActions : public QObject
{
    public:
    StateActions(iscore::ToplevelMenuElement, ScenarioApplicationPlugin* parent);

    void makeGUIElements(iscore::GUIElementsRef ref);


    void fillMenuBar(iscore::MenubarManager *menu) ;
    void fillContextMenu(QMenu* menu, const Selection&, const TemporalScenarioPresenter& pres, const QPoint&, const QPointF&) ;
    void setEnabled(bool) ;

    QList<QAction*> actions() const ;

    QAction* updateStates() const
    { return m_updateStates; }
    private:
    CommandDispatcher<> dispatcher();

    iscore::ToplevelMenuElement m_menuElt;
    ScenarioApplicationPlugin* m_parent{};
    QAction* m_updateStates{};
};
}
