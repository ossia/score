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
    ISCORE_LIB_BASE_EXPORT QString MenuInterface::name(ToplevelMenuElement elt)
    {
        return m_map.at(elt);
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

}
