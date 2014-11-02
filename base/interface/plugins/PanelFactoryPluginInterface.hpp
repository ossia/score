#pragma once
#include <QString>
#include <interface/panels/Panel.hpp>

namespace iscore
{
	class PanelFactoryPluginInterface
	{
		public:
			virtual ~PanelFactoryPluginInterface() = default;

			// List the panels offered by the plugin.
			virtual QStringList panel_list() const = 0;
			virtual Panel* panel_make(QString name) = 0;
	};
}

#define PanelFactoryPluginInterface_iid "org.ossia.i-score.plugins.PanelFactoryPluginInterface"

Q_DECLARE_INTERFACE(iscore::PanelFactoryPluginInterface, PanelFactoryPluginInterface_iid)
