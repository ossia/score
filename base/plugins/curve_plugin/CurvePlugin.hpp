#pragma once
#include <QObject>
#include <interface/plugins/Autoconnect_QtInterface.hpp>
#include <interface/plugins/CustomFactoryInterface_QtInterface.hpp>
#include <interface/plugins/PanelFactoryInterface_QtInterface.hpp>
#include <interface/plugins/PluginControlInterface_QtInterface.hpp>

class Dummy {};
class CurvePlugin:
	public QObject,
	public iscore::Autoconnect_QtInterface,
	public iscore::FactoryInterface_QtInterface,
	public iscore::PanelFactoryInterface_QtInterface,
	public iscore::PluginControlInterface_QtInterface,
	private Dummy
{
		Q_OBJECT
		Q_PLUGIN_METADATA (IID Autoconnect_QtInterface_iid)
		Q_INTERFACES (
		    iscore::Autoconnect_QtInterface
		    iscore::FactoryInterface_QtInterface
		    iscore::PanelFactoryInterface_QtInterface
		    iscore::PluginControlInterface_QtInterface
		)

	public:
		CurvePlugin();
		virtual ~CurvePlugin() = default;

		virtual QList<iscore::Autoconnect> autoconnect_list() const override;


		virtual QVector<iscore::FactoryInterface*> factories_make (QString factoryName) override;


		virtual QStringList panel_list() const override;
		virtual iscore::PanelFactoryInterface* panel_make (QString name) override;

		virtual QStringList control_list() const override;
		virtual iscore::PluginControlInterface* control_make (QString) override;

};

