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
		ViewMenu,
		SettingsMenu,
		AboutMenu
	};

	enum class FileMenuElement
	{
		New,
		Separator_Load,
		Load,
		Save,
		SaveAs,
		Separator_Export,
		Export,
		Separator_Quit,
		Quit
	};

	enum class ViewMenuElement
	{
		Windows
	};

	enum class SettingsMenuElement
	{
		Settings
	};

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
			static const std::map<ViewMenuElement, QString> m_viewMap;
			static const std::map<SettingsMenuElement, QString> m_settingsMap;
	};
}
