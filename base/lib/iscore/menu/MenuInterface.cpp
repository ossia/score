#include "MenuInterface.hpp"
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
    std::map<EditMenuElement, QString> MenuInterface::map()
    {
        return m_editMap;
    }

    template<>
    std::map<ToolMenuElement, QString> MenuInterface::map()
    {
        return m_toolMap;
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
    std::map<AboutMenuElement, QString> MenuInterface::map()
    {
        return m_aboutMap;
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
    QString MenuInterface::name(EditMenuElement elt)
    {
        return m_editMap.at(elt);
    }

    template<>
    QString MenuInterface::name(ToolMenuElement elt)
    {
        return m_toolMap.at(elt);
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

    template<>
    QString MenuInterface::name(AboutMenuElement elt)
    {
        return m_aboutMap.at(elt);
    }
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


const std::map<FileMenuElement, QString> MenuInterface::m_fileMap
{
    {FileMenuElement::New, QObject::tr("New") },
    {FileMenuElement::Separator_Load, QObject::tr("Separator_Load") },
    {FileMenuElement::Load, QObject::tr("Open") },
    {FileMenuElement::Recent, QObject::tr("Recent files") },
    {FileMenuElement::Save, QObject::tr("Save") },
    {FileMenuElement::SaveAs, QObject::tr("Save As") },
    {FileMenuElement::Separator_Export, QObject::tr("Separator_Export") },
    {FileMenuElement::Export, QObject::tr("Export") },
    {FileMenuElement::Separator_Quit, QObject::tr("Separator_Quit") },
    {FileMenuElement::Close, QObject::tr("Close") },
    {FileMenuElement::Quit, QObject::tr("Quit") }
};

const std::map<EditMenuElement, QString> MenuInterface::m_editMap
{
    {EditMenuElement::Separator_Copy, QObject::tr("Separator_Copy") },
    {EditMenuElement::Copy, QObject::tr("Copy") },
    {EditMenuElement::Cut, QObject::tr("Cut") },
    {EditMenuElement::Paste, QObject::tr("Paste") },
    {EditMenuElement::Separator_Undo, QObject::tr("Separator_Undo") },
    {EditMenuElement::Undo, QObject::tr("Undo") },
    {EditMenuElement::Redo, QObject::tr("Redo") }
};

const std::map<ToolMenuElement, QString> MenuInterface::m_toolMap
{
    {ToolMenuElement::Separator_Tool, QObject::tr("Separator_Tool") }
};

const std::map<ViewMenuElement, QString> MenuInterface::m_viewMap
{
    {ViewMenuElement::Windows, QObject::tr("Windows") }
};

const std::map<SettingsMenuElement, QString> MenuInterface::m_settingsMap
{
    {SettingsMenuElement::Settings, QObject::tr("Settings") }
};

const std::map<AboutMenuElement, QString> MenuInterface::m_aboutMap
{
    {AboutMenuElement::Help, QObject::tr("Help") },
    {AboutMenuElement::About, QObject::tr("About") },
};
