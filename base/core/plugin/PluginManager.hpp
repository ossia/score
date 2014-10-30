#pragma once
#include <QPluginLoader>
#include <QMap>

#include <core/processes/ProcessList.hpp>
#include <interface/autoconnect/Autoconnect.hpp>

#include <interface/plugins/CustomCommandFactoryPluginInterface.hpp>
#include <interface/plugins/AutoconnectFactoryPluginInterface.hpp>
#include <interface/plugins/PanelFactoryPluginInterface.hpp>
#include <interface/plugins/ProcessFactoryPluginInterface.hpp>
#include <interface/plugins/SettingsFactoryPluginInterface.hpp>

#include <interface/autoconnect/Autoconnect.hpp>

namespace iscore
{

	using CommandList = std::vector<CustomCommand*>;
	using PanelList = std::vector<Panel*>;
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
			using QObject::QObject;
			~PluginManager()
			{
				clearPlugins();
			}

			QMap<QString, QObject*> availablePlugins() const
			{
				return m_availablePlugins;
			}

			void reloadPlugins();

		private:
			void dispatch(QObject* plugin);
			void clearPlugins();
			QMap<QString, QObject*> m_availablePlugins;

			AutoconnectList m_autoconnections; // TODO try unordered_set
			ProcessList  m_processList;
			CommandList  m_commandList;
			PanelList    m_panelList;
			SettingsList m_settingsList;
	};
}
