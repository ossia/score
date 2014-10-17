#pragma once
#include <interface/customcommand/CustomCommand.hpp>

namespace iscore
{
	// Commands to add to the menu bars.
	class CustomCommandFactoryPluginInterface
	{
		public:

			virtual ~CustomCommandFactoryPluginInterface() = default;

			virtual QStringList customCommand_list() const = 0;
			virtual CustomCommand* customCommand_make(QString) = 0;
	};
}


#define CustomCommandFactoryPluginInterface_iid "org.ossia.i-score.plugins.CustomCommandFactoryPluginInterface"

Q_DECLARE_INTERFACE(iscore::CustomCommandFactoryPluginInterface, CustomCommandFactoryPluginInterface_iid)
