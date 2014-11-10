#pragma once

#include <interface/plugins/Autoconnect_QtInterface.hpp>
#include <interface/plugins/PluginControlInterface_QtInterface.hpp>
#include <interface/plugins/ProcessFactoryInterface_QtInterface.hpp>
#include <interface/plugins/PanelFactoryInterface_QtInterface.hpp>
#include <interface/plugins/DocumentDelegateFactoryInterface_QtInterface.hpp>
#include <QObject>

class SimpleProcessPlugin :
		public QObject,
		public iscore::Autoconnect_QtInterface,
		public iscore::PluginControlInterface_QtInterface,
		public iscore::ProcessFactoryInterface_QtInterface,
		public iscore::PanelFactoryInterface_QtInterface,
		public iscore::DocumentDelegateFactoryInterface_QtInterface
{
		Q_OBJECT
		Q_PLUGIN_METADATA(IID Autoconnect_QtInterface_iid)
		Q_INTERFACES(iscore::Autoconnect_QtInterface
					 iscore::PluginControlInterface_QtInterface
					 iscore::ProcessFactoryInterface_QtInterface
					 iscore::PanelFactoryInterface_QtInterface
					 iscore::DocumentDelegateFactoryInterface_QtInterface)

	public:
		SimpleProcessPlugin();
		virtual ~SimpleProcessPlugin() = default;

		// Autoconnect interface
		virtual QList<iscore::Autoconnect> autoconnect_list() const override;

		// Process interface
		virtual QStringList process_list() const override;
		virtual iscore::ProcessFactoryInterface* process_make(QString name) override;

		// CustomCommand interface
		virtual QStringList control_list() const override;
		virtual iscore::PluginControlInterface* control_make(QString) override;

		// Panel interface
		virtual QStringList panel_list() const override;
		virtual iscore::PanelFactoryInterface* panel_make(QString name) override;

		// Docpanel interface
		virtual QStringList document_list() const override;
		virtual iscore::DocumentDelegateFactoryInterface* document_make(QString name) override;
};
