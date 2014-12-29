#pragma once
#include <tools/NamedObject.hpp>
#include <QPluginLoader>
#include <QMap>

#include <interface/customfactory/FactoryFamily.hpp>
#include <interface/autoconnect/Autoconnect.hpp>

namespace iscore
{
	class PluginControlInterface;
	class PanelFactoryInterface;
	class DocumentDelegateFactoryInterface;
	class SettingsDelegateFactoryInterface;

	using FactoryFamilyList = QVector<FactoryFamily>;
	using CommandList = std::vector<PluginControlInterface*>;
	using PanelList = std::vector<PanelFactoryInterface*>;
	using DocumentPanelList = std::vector<DocumentDelegateFactoryInterface*>;
	using SettingsList = std::vector<SettingsDelegateFactoryInterface*>;
	using AutoconnectList = std::vector<Autoconnect>;

	/**
	 * @brief The PluginManager class loads and keeps track of the plug-ins.
	 */
	class PluginManager : public NamedObject
	{
			Q_OBJECT
			friend class Application;
		public:
			PluginManager(QObject* parent):
				NamedObject{"PluginManager", parent}
			{
			}

			~PluginManager()
			{
				clearPlugins();
			}

			QMap<QString, QObject*> availablePlugins() const
			{
				return m_availablePlugins;
			}

			void reloadPlugins();

			QStringList pluginsOnSystem() const
			{
				return m_pluginsOnSystem;
			}

		private:
			// We need a list for all the plug-ins present on the system even if we do not load them.
			// Else we can't blacklist / unblacklist plug-ins.
			QStringList m_pluginsOnSystem;

			void loadFactories(QObject* plugin);
			void dispatch(QObject* plugin);
			void clearPlugins();

			QStringList pluginsBlacklist();

			// Here, the plug-ins that are effectively loaded.
			QMap<QString, QObject*> m_availablePlugins;

			FactoryFamilyList m_customFactories;
			AutoconnectList m_autoconnections;
			CommandList  m_commandList;
			PanelList    m_panelList;
			DocumentPanelList m_documentPanelList;
			SettingsList m_settingsList;
	};
}
