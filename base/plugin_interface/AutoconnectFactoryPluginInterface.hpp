#pragma once
#include <interface/autoconnect/Autoconnect.hpp>

namespace iscore
{
	class AutoconnectFactoryPluginInterface
	{
		public:
			virtual ~AutoconnectFactoryPluginInterface() = default;
			// List the Processes offered by the plugin.

			virtual QList<iscore::Autoconnect> autoconnect_list() const = 0;
	};
}

#define AutoconnectFactoryPluginInterface_iid "org.ossia.i-score.plugins.AutoconnectFactoryPluginInterface"

Q_DECLARE_INTERFACE(iscore::AutoconnectFactoryPluginInterface, AutoconnectFactoryPluginInterface_iid)
