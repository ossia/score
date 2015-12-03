#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include "ScenarioActions.hpp"
#include <iscore/menu/MenuInterface.hpp>

ScenarioActions::ScenarioActions(
        iscore::ToplevelMenuElement menuElt,
        ScenarioApplicationPlugin *parent) :
    QObject{parent},
    m_parent{parent},
    m_menuElt{menuElt}
{

}

ScenarioActions::~ScenarioActions()
{

}

QList<QAction*> ScenarioActions::actions() const
{
    return {};
}

