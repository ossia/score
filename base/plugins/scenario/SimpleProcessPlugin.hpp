#pragma once

#include <interface/plugins/Autoconnect_QtInterface.hpp>
#include <interface/plugins/PluginControlInterface_QtInterface.hpp>
#include <interface/plugins/ProcessFactoryInterface_QtInterface.hpp>
#include <interface/plugins/SettingsDelegateFactoryInterface_QtInterface.hpp>
#include <interface/plugins/DocumentDelegateFactoryInterface_QtInterface.hpp>
#include <QObject>
class ScenarioSettings;

/* TODO @Nico
 * 
 * Renommer en ScenarioPlugin :)
 */
class SimpleProcessPlugin :
		public QObject,
		public iscore::Autoconnect_QtInterface,
		public iscore::PluginControlInterface_QtInterface,
		public iscore::ProcessFactoryInterface_QtInterface,
		public iscore::SettingsDelegateFactoryInterface_QtInterface,
		public iscore::DocumentDelegateFactoryInterface_QtInterface
{
		Q_OBJECT
		Q_PLUGIN_METADATA(IID Autoconnect_QtInterface_iid)
		Q_INTERFACES(iscore::Autoconnect_QtInterface
					 iscore::PluginControlInterface_QtInterface
					 iscore::ProcessFactoryInterface_QtInterface
					 iscore::SettingsDelegateFactoryInterface_QtInterface
					 iscore::DocumentDelegateFactoryInterface_QtInterface)

	public:
		SimpleProcessPlugin();
		virtual ~SimpleProcessPlugin() = default;

		// Autoconnect interface
		virtual QList<iscore::Autoconnect> autoconnect_list() const override;

		// Process interface
		virtual QStringList process_list() const override;
		virtual iscore::ProcessFactoryInterface* process_make(QString name) override;

		// Settings interface
		virtual iscore::SettingsDelegateFactoryInterface* settings_make() override;

		// CustomCommand interface
		virtual QStringList control_list() const override;
		virtual iscore::PluginControlInterface* control_make(QString) override;

		// Docpanel interface
		virtual QStringList document_list() const override;
		virtual iscore::DocumentDelegateFactoryInterface* document_make(QString name) override;
};
