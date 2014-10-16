#pragma once

#include <plugin_interface/ProcessFactoryPluginInterface.hpp>
#include <plugin_interface/SettingsFactoryPluginInterface.hpp>
#include <QObject>

class SimpleProcessPlugin :
		public QObject,
		public iscore::ProcessFactoryPluginInterface,
		public iscore::SettingsFactoryPluginInterface
{
		Q_OBJECT
		Q_PLUGIN_METADATA(IID ProcessFactoryPluginInterface_iid)
		Q_INTERFACES(iscore::ProcessFactoryPluginInterface
					 iscore::SettingsFactoryPluginInterface)

	public:
		SimpleProcessPlugin():
			QObject{},
			iscore::ProcessFactoryPluginInterface{},
			iscore::SettingsFactoryPluginInterface{}
		{
			setObjectName("SimpleProcessPlugin");
		}

		// Process interface
		virtual QStringList process_list() const override;
		virtual std::unique_ptr<iscore::Process> process_make(QString name) override;

		// Settings interface
		virtual std::unique_ptr<iscore::SettingsGroup> settings_make() override;
};
