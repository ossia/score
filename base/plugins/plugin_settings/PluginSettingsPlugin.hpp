#pragma once
#include <interface/plugins/AutoconnectFactoryPluginInterface.hpp>
#include <interface/plugins/SettingsFactoryPluginInterface.hpp>
#include <QObject>

class PluginSettingsPlugin :
		public QObject,
		public iscore::AutoconnectFactoryPluginInterface,
		public iscore::SettingsFactoryPluginInterface
{
		Q_OBJECT
		Q_PLUGIN_METADATA(IID AutoconnectFactoryPluginInterface_iid)
		Q_INTERFACES(iscore::AutoconnectFactoryPluginInterface
					 iscore::SettingsFactoryPluginInterface)

	public:
		PluginSettingsPlugin();
		virtual ~PluginSettingsPlugin() = default;

		// Autoconnect interface
		virtual QList<iscore::Autoconnect> autoconnect_list() const override;

		// Settings interface
		virtual iscore::SettingsGroup* settings_make() override;
};
