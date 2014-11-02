#pragma once
#include <interface/settings/SettingsGroup.hpp>

namespace iscore
{
	// A plug-in can also offer global settings for everything it provides.
	// How to make the elements react to the settings ?
	class SettingsFactoryPluginInterface
	{
		public:
			virtual ~SettingsFactoryPluginInterface() = default;
			virtual SettingsGroup* settings_make() = 0;
	};
}


#define SettingsFactoryPluginInterface_iid "org.ossia.i-score.plugins.SettingsFactoryPluginInterface"

Q_DECLARE_INTERFACE(iscore::SettingsFactoryPluginInterface, SettingsFactoryPluginInterface_iid)
