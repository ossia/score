#pragma once
#include <map>
#include <QString>
#include <QObject>

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

	enum class ViewMenuElement
	{
		Windows
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
			static const std::map<ViewMenuElement, QString> m_viewMap;
	};
}
