#pragma once
#include <interface/plugins/PanelFactoryInterface_QtInterface.hpp>
#include <interface/plugins/Autoconnect_QtInterface.hpp>
#include <interface/plugins/FactoryInterface_QtInterface.hpp>
#include <interface/plugins/PluginControlInterface_QtInterface.hpp>
#include <interface/plugins/FactoryFamily_QtInterface.hpp>

class InspectorControl;
class InspectorPlugin :
	public QObject,
	public iscore::Autoconnect_QtInterface,
	public iscore::PanelFactoryInterface_QtInterface,
	public iscore::PluginControlInterface_QtInterface,
	public iscore::FactoryFamily_QtInterface
{
		Q_OBJECT
		Q_PLUGIN_METADATA (IID Autoconnect_QtInterface_iid)
		Q_INTERFACES (iscore::Autoconnect_QtInterface
					  iscore::PanelFactoryInterface_QtInterface
					  iscore::FactoryFamily_QtInterface
					  iscore::PluginControlInterface_QtInterface)

	public:
		InspectorPlugin();
		virtual ~InspectorPlugin() = default;

		// Autoconnect interface
		virtual QList<iscore::Autoconnect> autoconnect_list() const override;

		// Panel interface
		virtual QStringList panel_list() const override;
		virtual iscore::PanelFactoryInterface* panel_make (QString name) override;

		// Offre la factory de Inspector
		virtual QVector<iscore::FactoryFamily> factoryFamilies_make() override;

		virtual QStringList control_list() const;
		virtual iscore::PluginControlInterface*control_make(QString);

	private:
		InspectorControl* m_inspectorControl;
};
