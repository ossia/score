#include <interface/customcommand/MenuInterface.hpp>

using namespace iscore;

namespace iscore
{
	template<>
	std::map<ToplevelMenuElement, QString> MenuInterface::map()
	{
		return m_map;
	}

	template<>
	std::map<ViewMenuElement, QString> MenuInterface::map()
	{
		return m_viewMap;
	}

	template<>
	QString MenuInterface::name(ToplevelMenuElement elt)
	{
		return m_map.at(elt);
	}

	template<>
	QString MenuInterface::name(ViewMenuElement elt)
	{
		return m_viewMap.at(elt);
	}
}

const std::map<ToplevelMenuElement, QString> MenuInterface::m_map
{
	{ToplevelMenuElement::FileMenu, QObject::tr("File")},
	{ToplevelMenuElement::EditMenu, QObject::tr("Edit")},
	{ToplevelMenuElement::ViewMenu, QObject::tr("View")},
	{ToplevelMenuElement::SettingsMenu, QObject::tr("Settings")},
	{ToplevelMenuElement::AboutMenu, QObject::tr("About")}
};

const std::map<ViewMenuElement, QString> MenuInterface::m_viewMap
{
	{ViewMenuElement::Windows, QObject::tr("Windows")}
};
