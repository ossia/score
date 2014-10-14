#pragma once
#include <QString>
#include <interface/panels/Panel.h>

namespace iscore
{
	class PanelFactoryPluginInterface
	{
		public:
			virtual ~PanelFactoryPluginInterface() = default;
			
			// List the panels offered by the plugin.
            virtual QStringList panel_list() const = 0;
			virtual std::unique_ptr<Panel> panel_make(QString name) = 0;
	};
}
