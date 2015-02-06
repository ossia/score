#pragma once
#include <QObject>
#include <plugins/FactoryInterface_QtInterface.hpp>
#include <interface/plugins/PluginControlInterface_QtInterface.hpp>

class CurvePlugin:
		public QObject,
		public iscore::FactoryInterface_QtInterface,
		public iscore::PluginControlInterface_QtInterface
{
		Q_OBJECT
		Q_PLUGIN_METADATA (IID FactoryInterface_QtInterface_iid)
		Q_INTERFACES (
				iscore::FactoryInterface_QtInterface
				iscore::PluginControlInterface_QtInterface
				)

	public:
		CurvePlugin();
		virtual ~CurvePlugin() = default;

		// Plugin control interface
		virtual QStringList control_list() const override;
		virtual iscore::PluginControlInterface* control_make(QString) override;

		// Process & inspector
		virtual QVector<iscore::FactoryInterface*> factories_make (QString factoryName) override;
};

