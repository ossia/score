#include "AbstractMenuActions.hpp"
#include "Control/ScenarioControl.hpp"
AbstractMenuActions::AbstractMenuActions(iscore::ToplevelMenuElement menuElt, ScenarioControl *parent) :
    QObject(parent),
    m_parent{parent},
    m_menuElt{menuElt}
{

}

