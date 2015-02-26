#include <interface/plugincontrol/MenuInterface.hpp>
using namespace iscore;

namespace iscore
{
    template<>
    std::map<ToplevelMenuElement, QString> MenuInterface::map()
    {
        return m_map;
    }

    template<>
    std::map<FileMenuElement, QString> MenuInterface::map()
    {
        return m_fileMap;
    }

    template<>
    std::map<ViewMenuElement, QString> MenuInterface::map()
    {
        return m_viewMap;
    }

    template<>
    std::map<SettingsMenuElement, QString> MenuInterface::map()
    {
        return m_settingsMap;
    }



    template<>
    QString MenuInterface::name(ToplevelMenuElement elt)
    {
        return m_map.at(elt);
    }

    template<>
    QString MenuInterface::name(FileMenuElement elt)
    {
        return m_fileMap.at(elt);
    }

    template<>
    QString MenuInterface::name(ViewMenuElement elt)
    {
        return m_viewMap.at(elt);
    }

    template<>
    QString MenuInterface::name(SettingsMenuElement elt)
    {
        return m_settingsMap.at(elt);
    }
}

const std::map<ToplevelMenuElement, QString> MenuInterface::m_map
{
    {ToplevelMenuElement::FileMenu, QObject::tr("File") },
    {ToplevelMenuElement::EditMenu, QObject::tr("Edit") },
    {ToplevelMenuElement::ViewMenu, QObject::tr("View") },
    {ToplevelMenuElement::SettingsMenu, QObject::tr("Settings") },
    {ToplevelMenuElement::AboutMenu, QObject::tr("About") }
};


const std::map<FileMenuElement, QString> MenuInterface::m_fileMap
{
    {FileMenuElement::New, QObject::tr("New") },
    {FileMenuElement::Separator_Load, QObject::tr("Separator_Load") },
    {FileMenuElement::Load, QObject::tr("Load") },
    {FileMenuElement::Save, QObject::tr("Save") },
    {FileMenuElement::SaveAs, QObject::tr("Save As") },
    {FileMenuElement::Separator_Export, QObject::tr("Separator_Export") },
    {FileMenuElement::Export, QObject::tr("Export") },
    {FileMenuElement::Separator_Quit, QObject::tr("Separator_Quit") },
    {FileMenuElement::Quit, QObject::tr("Quit") }
};

const std::map<ViewMenuElement, QString> MenuInterface::m_viewMap
{
    {ViewMenuElement::Windows, QObject::tr("Windows") }
};

const std::map<SettingsMenuElement, QString> MenuInterface::m_settingsMap
{
    {SettingsMenuElement::Settings, QObject::tr("Settings") }
};
