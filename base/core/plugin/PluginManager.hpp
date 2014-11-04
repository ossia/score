#pragma once
#include <QPluginLoader>
#include <QMap>

#include <core/processes/ProcessList.hpp>
#include <interface/autoconnect/Autoconnect.hpp>

#include <interface/plugins/CustomCommandFactoryPluginInterface.hpp>
#include <interface/plugins/AutoconnectFactoryPluginInterface.hpp>
#include <interface/plugins/PanelFactoryPluginInterface.hpp>
#include <interface/plugins/DocumentPanelFactoryPluginInterface.hpp>
#include <interface/plugins/ProcessFactoryPluginInterface.hpp>
#include <interface/plugins/SettingsFactoryPluginInterface.hpp>

#include <interface/autoconnect/Autoconnect.hpp>

namespace iscore
{

	using CommandList = std::vector<CustomCommand*>;
	using PanelList = std::vector<Panel*>;
	using DocumentPanelList = std::vector<DocumentPanel*>;
	using SettingsList = std::vector<SettingsGroup*>;
	using AutoconnectList = std::vector<Autoconnect>;

	/**
	 * @brief The PluginManager class loads and keeps track of the plug-ins.
	 */
	class PluginManager : public QObject
	{
			Q_OBJECT
			friend class Application;
		public:
			PluginManager(QObject* parent): QObject{parent}
			{ this->setObjectName("PluginManager"); }

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

			void dispatch(QObject* plugin);
			void clearPlugins();

			QStringList pluginsBlacklist();

			// Here, the plug-ins that are effectively loaded.
			QMap<QString, QObject*> m_availablePlugins;

			AutoconnectList m_autoconnections; // TODO try unordered_set
			ProcessList  m_processList;
			CommandList  m_commandList;
			PanelList    m_panelList;
			DocumentPanelList m_documentPanelList;
			SettingsList m_settingsList;
	};
}
