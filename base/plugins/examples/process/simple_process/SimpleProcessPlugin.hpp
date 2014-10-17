#pragma once

#include <plugin_interface/AutoconnectFactoryPluginInterface.hpp>
#include <plugin_interface/ProcessFactoryPluginInterface.hpp>
#include <plugin_interface/SettingsFactoryPluginInterface.hpp>
#include <QObject>
class HelloWorldSettings;

class SimpleProcessPlugin :
		public QObject,
		public iscore::AutoconnectFactoryPluginInterface,
		public iscore::ProcessFactoryPluginInterface,
		public iscore::SettingsFactoryPluginInterface
{
		Q_OBJECT
		Q_PLUGIN_METADATA(IID ProcessFactoryPluginInterface_iid)
		Q_INTERFACES(iscore::AutoconnectFactoryPluginInterface
					 iscore::ProcessFactoryPluginInterface
					 iscore::SettingsFactoryPluginInterface)

	public:
		SimpleProcessPlugin():
			QObject{},
			iscore::AutoconnectFactoryPluginInterface{},
			iscore::ProcessFactoryPluginInterface{},
			iscore::SettingsFactoryPluginInterface{}
		{
			setObjectName("SimpleProcessPlugin");
		}
		
		// Autoconnect interface
		virtual QList<iscore::Autoconnect> autoconnect_list() const override;
		
		// Process interface
		virtual QStringList process_list() const override;
		virtual iscore::Process* process_make(QString name) override;

		// Settings interface
		virtual iscore::SettingsGroup* settings_make() override;

	private:
		HelloWorldSettings* m_currentSettings;
};
