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
	
	class MenuInterface
	{
		public:
			QString name(ToplevelMenuElement elt)  const 
			{
				return m_map.at(elt);
			}
			
			const std::map<ToplevelMenuElement, QString> m_map
			{
				{ToplevelMenuElement::FileMenu, QObject::tr("File")},
				{ToplevelMenuElement::EditMenu, QObject::tr("Edit")},
				{ToplevelMenuElement::ViewMenu, QObject::tr("View")},
				{ToplevelMenuElement::SettingsMenu, QObject::tr("Settings")},
				{ToplevelMenuElement::AboutMenu, QObject::tr("About")}
			};
	};
}
