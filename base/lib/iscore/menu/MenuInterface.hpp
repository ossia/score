#pragma once
#include <map>
#include <QString>
#include <QObject>

// TODO faire un script qui génère ça automatiquement
namespace iscore
{
    enum class ToplevelMenuElement
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

    enum class FileMenuElement
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
        Quit
    };

    enum class EditMenuElement
    {
        Separator_Copy,
        Copy,
        Cut,
        Paste,
        Separator_Undo,
        Undo,
        Redo
    };

    enum class ToolMenuElement
    {
        Separator_Tool
    };

    enum class ViewMenuElement
    {
        Windows
    };

    enum class SettingsMenuElement
    {
        Settings
    };


    enum class AboutMenuElement
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
    class MenuInterface
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
