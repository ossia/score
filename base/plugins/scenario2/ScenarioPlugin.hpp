#pragma once
#include <QObject>
#include <interface/plugins/DocumentDelegateFactoryInterface_QtInterface.hpp>

class ScenarioPlugin :
		public QObject,
		public iscore::DocumentDelegateFactoryInterface_QtInterface
{
		Q_OBJECT
		Q_PLUGIN_METADATA(IID DocumentDelegateFactoryInterface_QtInterface_iid)
		Q_INTERFACES(iscore::DocumentDelegateFactoryInterface_QtInterface)

	public:
		ScenarioPlugin();
		virtual ~ScenarioPlugin() = default;

		// Docpanel interface
		virtual QStringList document_list() const override;
		virtual iscore::DocumentDelegateFactoryInterface* document_make(QString name) override;
};
