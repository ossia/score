#pragma once
#include <interface/settings/SettingsGroup.h>

namespace iscore
{
	// A plug-in can also offer global settings for everything it provides.
	// How to make the elements react to the settings ?
	class SettingsFactoryPluginInterface
	{
		virtual ~SettingsFactoryPluginInterface() = default;		
        virtual std::unique_ptr<SettingsGroup> settings_make() = 0;
	};
}
