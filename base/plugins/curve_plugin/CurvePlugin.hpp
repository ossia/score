#pragma once
#include <QObject>
#include <plugins/CustomFactoryInterface_QtInterface.hpp>

class CurvePlugin:
	public QObject,
	public iscore::FactoryInterface_QtInterface
{
		Q_OBJECT
		Q_PLUGIN_METADATA (IID FactoryInterface_QtInterface_iid)
		Q_INTERFACES (
			iscore::FactoryInterface_QtInterface
		)

	public:
		CurvePlugin();
		virtual ~CurvePlugin() = default;

		virtual QVector<iscore::FactoryInterface*> factories_make (QString factoryName) override;
};

