#include <QObject>
#include "MenuInterface.hpp"


namespace iscore
{
    template<>
    ISCORE_LIB_BASE_EXPORT std::map<ToplevelMenuElement, QString> MenuInterface::map()
    {
        return m_map;
    }

    template<>
    ISCORE_LIB_BASE_EXPORT std::map<ContextMenu, QString> MenuInterface::map()
    {
        return m_contextMap;
    }



    template<>
    ISCORE_LIB_BASE_EXPORT QString MenuInterface::name(ToplevelMenuElement elt)
    {
        return m_map.at(elt);
    }

    template<>
    ISCORE_LIB_BASE_EXPORT QString MenuInterface::name(ContextMenu elt)
    {
        return m_contextMap.at(elt);
    }

const std::map<ToplevelMenuElement, QString> MenuInterface::m_map
{
    {ToplevelMenuElement::FileMenu, QObject::tr("File") },
    {ToplevelMenuElement::EditMenu, QObject::tr("Edit") },
    {ToplevelMenuElement::ObjectMenu, QObject::tr("Object") },
    {ToplevelMenuElement::PlayMenu, QObject::tr("Play") },
    {ToplevelMenuElement::ToolMenu, QObject::tr("Tool") },
    {ToplevelMenuElement::ViewMenu, QObject::tr("View") },
    {ToplevelMenuElement::SettingsMenu, QObject::tr("Settings") },
    {ToplevelMenuElement::AboutMenu, QObject::tr("About") }
};


const std::map<ContextMenu, QString> MenuInterface::m_contextMap
{
    {ContextMenu::Object, QObject::tr("Object") },
    {ContextMenu::Constraint, QObject::tr("Constraint") },
    {ContextMenu::Process, QObject::tr("Process") },
    {ContextMenu::Slot, QObject::tr("Slot") },
    {ContextMenu::Rack, QObject::tr("Rack") },
    {ContextMenu::Event, QObject::tr("Event") },
    {ContextMenu::State, QObject::tr("State") },
};
}
