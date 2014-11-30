#pragma once
#include <QObject>
#include <interface/plugins/DocumentDelegateFactoryInterface_QtInterface.hpp>
#include <interface/plugins/PluginControlInterface_QtInterface.hpp>
#include <interface/plugins/ProcessFactoryInterface_QtInterface.hpp>
#include <interface/plugins/Autoconnect_QtInterface.hpp>

class ScenarioPlugin :
		public QObject,
		public iscore::Autoconnect_QtInterface,
		public iscore::PluginControlInterface_QtInterface,
		public iscore::DocumentDelegateFactoryInterface_QtInterface,
		public iscore::ProcessFactoryInterface_QtInterface
{
		Q_OBJECT
		Q_PLUGIN_METADATA(IID DocumentDelegateFactoryInterface_QtInterface_iid)
		Q_INTERFACES(iscore::Autoconnect_QtInterface
					 iscore::PluginControlInterface_QtInterface
					 iscore::DocumentDelegateFactoryInterface_QtInterface
					 iscore::ProcessFactoryInterface_QtInterface)

	public:
		ScenarioPlugin();
		virtual ~ScenarioPlugin() = default;

		// Autoconnect interface
		virtual QList<iscore::Autoconnect> autoconnect_list() const override;

		// Docpanel interface
		virtual QStringList document_list() const override;
		virtual iscore::DocumentDelegateFactoryInterface* document_make(QString name) override;

		// Process interface
		virtual QStringList process_list() const override;
		virtual iscore::ProcessFactoryInterface* process_make(QString name) override;

		// Plugin control interface
		virtual QStringList control_list() const override;
		virtual iscore::PluginControlInterface* control_make(QString) override;

};
