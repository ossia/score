#pragma once
#include <QString>
#include <map>
#include <iscore_lib_base_export.h>

// TODO faire un script qui génère ça automatiquement
namespace iscore
{
    enum class ToplevelMenuElement : int // ISCORE_LIB_BASE_EXPORT
    {
        FileMenu,
        EditMenu,
        ObjectMenu,
        PlayMenu,
        ToolMenu,
        ViewMenu,
        SettingsMenu,
        AboutMenu
    };

    enum class FileMenuElement : int // ISCORE_LIB_BASE_EXPORT
    {
        New,
        Separator_Load,
        Load,
        Recent,
        Save,
        SaveAs,
        Separator_Export,
        Export,
        Separator_Quit,
        Close,
        Quit,
        SaveCommands,
        LoadCommands
    };

    enum class EditMenuElement : int // ISCORE_LIB_BASE_EXPORT
    {
        Separator_Copy,
        Copy,
        Cut,
        Paste,
        Separator_Undo,
        Undo,
        Redo
    };

    enum class ToolMenuElement : int // ISCORE_LIB_BASE_EXPORT
    {
        Separator_Tool
    };

    enum class ViewMenuElement : int // ISCORE_LIB_BASE_EXPORT
    {
        Windows
    };

    enum class SettingsMenuElement : int // ISCORE_LIB_BASE_EXPORT
    {
        Settings
    };


    enum class AboutMenuElement : int // ISCORE_LIB_BASE_EXPORT
    {
        Help,
        About
    };

    /**
     * @brief The MenuInterface class
     *
     * It is a way to allow plug-ins to put their options in a sensible place.
     * For instance, add an "Export" option after the standard "Save as...".
     *
     * The strings are not directly available to the plug-ins because they have to be translated.
     */
    class ISCORE_LIB_BASE_EXPORT MenuInterface
    {
        public:
            template<typename MenuType>
            static std::map<MenuType, QString> map();

            template<typename MenuType>
            static QString name(MenuType elt);

        private:
            static const std::map<ToplevelMenuElement, QString> m_map;
            static const std::map<FileMenuElement, QString> m_fileMap;
            static const std::map<EditMenuElement, QString> m_editMap;
            static const std::map<ToolMenuElement, QString> m_toolMap;
            static const std::map<ViewMenuElement, QString> m_viewMap;
            static const std::map<SettingsMenuElement, QString> m_settingsMap;
            static const std::map<AboutMenuElement, QString> m_aboutMap;
    };
}
