#pragma once
#include <interface/plugins/Autoconnect_QtInterface.hpp>
#include <interface/plugins/SettingsDelegateFactoryInterface_QtInterface.hpp>
#include <QObject>

class PluginSettingsPlugin :
		public QObject,
		public iscore::Autoconnect_QtInterface,
		public iscore::SettingsDelegateFactoryInterface_QtInterface
{
		Q_OBJECT
		Q_PLUGIN_METADATA(IID Autoconnect_QtInterface_iid)
		Q_INTERFACES(iscore::Autoconnect_QtInterface
					 iscore::SettingsDelegateFactoryInterface_QtInterface)

	public:
		PluginSettingsPlugin();
		virtual ~PluginSettingsPlugin() = default;

		// Autoconnect interface
		virtual QList<iscore::Autoconnect> autoconnect_list() const override;

		// Settings interface
		virtual iscore::SettingsDelegateFactoryInterface* settings_make() override;
};
