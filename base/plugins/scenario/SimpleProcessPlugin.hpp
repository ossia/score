#pragma once

#include <interface/plugins/AutoconnectFactoryPluginInterface.hpp>
#include <interface/plugins/CustomCommandFactoryPluginInterface.hpp>
#include <interface/plugins/ProcessFactoryPluginInterface.hpp>
#include <interface/plugins/SettingsFactoryPluginInterface.hpp>
#include <interface/plugins/DocumentPanelFactoryPluginInterface.hpp>
#include <QObject>
class ScenarioSettings;

class SimpleProcessPlugin :
		public QObject,
		public iscore::AutoconnectFactoryPluginInterface,
		public iscore::CustomCommandFactoryPluginInterface,
		public iscore::ProcessFactoryPluginInterface,
		public iscore::SettingsFactoryPluginInterface,
		public iscore::DocumentPanelFactoryPluginInterface
{
		Q_OBJECT
		Q_PLUGIN_METADATA(IID ProcessFactoryPluginInterface_iid)
		Q_INTERFACES(iscore::AutoconnectFactoryPluginInterface
					 iscore::CustomCommandFactoryPluginInterface
					 iscore::ProcessFactoryPluginInterface
					 iscore::SettingsFactoryPluginInterface
					 iscore::DocumentPanelFactoryPluginInterface)

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

		// Docpanel interface
		virtual QStringList document_list() const override;
		virtual iscore::DocumentPanel* document_make(QString name) override;
};
