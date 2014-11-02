#pragma once

#include <interface/plugins/AutoconnectFactoryPluginInterface.hpp>
#include <interface/plugins/CustomCommandFactoryPluginInterface.hpp>
#include <interface/plugins/PanelFactoryPluginInterface.hpp>
#include <interface/plugins/SettingsFactoryPluginInterface.hpp>
#include <QObject>
class NetworkSettings;

class NetworkPlugin :
		public QObject,
		public iscore::AutoconnectFactoryPluginInterface,
		public iscore::CustomCommandFactoryPluginInterface,
//		public iscore::PanelFactoryPluginInterface,
		public iscore::SettingsFactoryPluginInterface
{
		Q_OBJECT
		Q_PLUGIN_METADATA(IID AutoconnectFactoryPluginInterface_iid)
		Q_INTERFACES(iscore::AutoconnectFactoryPluginInterface
					 iscore::CustomCommandFactoryPluginInterface
//					 iscore::PanelFactoryPluginInterface
					 iscore::SettingsFactoryPluginInterface)

	public:
		NetworkPlugin();
		virtual ~NetworkPlugin() = default;

		// Autoconnect interface
		virtual QList<iscore::Autoconnect> autoconnect_list() const override;

		// Settings interface
		virtual iscore::SettingsGroup* settings_make() override;

		// CustomCommand interface
		virtual QStringList customCommand_list() const override;
		virtual iscore::CustomCommand* customCommand_make(QString) override;

		/* Pour les groupes
		// Panel interface
		virtual QStringList panel_list() const override;
		virtual iscore::Panel* panel_make(QString name) override;
		*/
};
