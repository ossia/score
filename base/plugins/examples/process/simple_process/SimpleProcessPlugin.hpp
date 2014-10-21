#pragma once

#include <plugin_interface/AutoconnectFactoryPluginInterface.hpp>
#include <plugin_interface/CustomCommandFactoryPluginInterface.hpp>
#include <plugin_interface/ProcessFactoryPluginInterface.hpp>
#include <plugin_interface/PanelFactoryPluginInterface.hpp>
#include <plugin_interface/SettingsFactoryPluginInterface.hpp>
#include <QObject>
class HelloWorldSettings;

class SimpleProcessPlugin :
		public QObject,
		public iscore::AutoconnectFactoryPluginInterface,
		public iscore::CustomCommandFactoryPluginInterface,
		public iscore::ProcessFactoryPluginInterface,
		public iscore::PanelFactoryPluginInterface,
		public iscore::SettingsFactoryPluginInterface
{
		Q_OBJECT
		Q_PLUGIN_METADATA(IID ProcessFactoryPluginInterface_iid)
		Q_INTERFACES(iscore::AutoconnectFactoryPluginInterface
					 iscore::CustomCommandFactoryPluginInterface
					 iscore::ProcessFactoryPluginInterface
					 iscore::PanelFactoryPluginInterface
					 iscore::SettingsFactoryPluginInterface)

	public:
		SimpleProcessPlugin();
		virtual ~SimpleProcessPlugin() = default;

		// Autoconnect interface
		virtual QList<iscore::Autoconnect> autoconnect_list() const override;

		// Process interface
		virtual QStringList process_list() const override;
		virtual iscore::Process* process_make(QString name) override;

		// Settings interface
		virtual iscore::SettingsGroup* settings_make() override;

		// CustomCommand interface
		virtual QStringList customCommand_list() const override;
		virtual iscore::CustomCommand* customCommand_make(QString) override;
		
		// Panel interface
		virtual QStringList panel_list() const override;
		virtual iscore::Panel* panel_make(QString name) override;
};
