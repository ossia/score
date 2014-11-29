#pragma once
#include <QObject>
#include <interface/plugins/DocumentDelegateFactoryInterface_QtInterface.hpp>
#include <interface/plugins/ProcessFactoryInterface_QtInterface.hpp>

class ScenarioPlugin :
		public QObject,
		public iscore::DocumentDelegateFactoryInterface_QtInterface,
		public iscore::ProcessFactoryInterface_QtInterface
{
		Q_OBJECT
		Q_PLUGIN_METADATA(IID DocumentDelegateFactoryInterface_QtInterface_iid)
		Q_INTERFACES(iscore::DocumentDelegateFactoryInterface_QtInterface
					 iscore::ProcessFactoryInterface_QtInterface)

	public:
		ScenarioPlugin();
		virtual ~ScenarioPlugin() = default;

		// Docpanel interface
		virtual QStringList document_list() const override;
		virtual iscore::DocumentDelegateFactoryInterface* document_make(QString name) override;

		// Process interface
		virtual QStringList process_list() const override;
		virtual iscore::ProcessFactoryInterface* process_make(QString name) override;
};
