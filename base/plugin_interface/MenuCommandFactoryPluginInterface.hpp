#pragma once
#include <interface/menu/MenuCommand.hpp>

namespace iscore
{
	// Commands to add to the menu bars.
	class MenuCommandFactoryPluginInterface
	{
		virtual ~MenuCommandFactoryPluginInterface() = default;

		virtual QStringList menuCommand_list() const = 0;
		virtual MenuCommand* menuCommand_make(QString) = 0;
	};
}
