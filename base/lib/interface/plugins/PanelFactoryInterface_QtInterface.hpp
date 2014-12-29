#pragma once
#include <QObject>

namespace iscore
{
	class PanelFactoryInterface;
	class PanelFactoryInterface_QtInterface
	{
		public:
			virtual ~PanelFactoryInterface_QtInterface() = default;

			// List the panels offered by the plugin.
			virtual QStringList panel_list() const = 0;
			virtual PanelFactoryInterface* panel_make(QString name) = 0;
	};
}

#define PanelFactoryInterface_QtInterface_iid "org.ossia.i-score.plugins.PanelFactoryInterface_QtInterface"

Q_DECLARE_INTERFACE(iscore::PanelFactoryInterface_QtInterface, PanelFactoryInterface_QtInterface_iid)
