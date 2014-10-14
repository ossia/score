#pragma once
#include <interface/menu/MenuCommand.h>

namespace iscore
{
	// Commands to add to the menu bars.
	class MenuCommandFactoryPluginInterface
	{
		virtual ~MenuCommandFactoryPluginInterface() = default;
		
        virtual QStringList menuCommand_list() const = 0;
		virtual std::unique_ptr<MenuCommand> menuCommand_make(QString) = 0;
	};
}
