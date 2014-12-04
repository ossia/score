#pragma once
#include <interface/plugincontrol/PluginControlInterface.hpp>

namespace iscore
{
	// Commands to add to the menu bars.
	class PluginControlInterface_QtInterface
	{
		public:

			virtual ~PluginControlInterface_QtInterface() = default;

			virtual QStringList control_list() const = 0;
			virtual PluginControlInterface* control_make(QString) = 0;
	};
}


#define PluginControlInterface_QtInterface_iid "org.ossia.i-score.plugins.PluginControlInterface_QtInterface"

Q_DECLARE_INTERFACE(iscore::PluginControlInterface_QtInterface, PluginControlInterface_QtInterface_iid)
