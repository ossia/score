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
			QString name(ToplevelMenuElement elt)  const 
			{
				return m_map.at(elt);
			}
			
			QString name(ViewMenuElement elt) const
			{
				return m_viewMap.at(elt);
			}
			
			const std::map<ToplevelMenuElement, QString> m_map
			{
				{ToplevelMenuElement::FileMenu, QObject::tr("File")},
				{ToplevelMenuElement::EditMenu, QObject::tr("Edit")},
				{ToplevelMenuElement::ViewMenu, QObject::tr("View")},
				{ToplevelMenuElement::SettingsMenu, QObject::tr("Settings")},
				{ToplevelMenuElement::AboutMenu, QObject::tr("About")}
			};
			
			const std::map<ViewMenuElement, QString> m_viewMap
			{
				{ViewMenuElement::Windows, QObject::tr("Windows")}
			};
	};
}
