#pragma once
#include <interface/settingsdelegate/SettingsDelegateFactoryInterface.hpp>

namespace iscore
{
	// A plug-in can also offer global settings for everything it provides.
	// How to make the elements react to the settings ?
	class SettingsDelegateFactoryInterface_QtInterface
	{
		public:
			virtual ~SettingsDelegateFactoryInterface_QtInterface() = default;
			virtual SettingsDelegateFactoryInterface* settings_make() = 0;
	};
}


#define SettingsDelegateFactoryInterface_QtInterface_iid "org.ossia.i-score.plugins.SettingsDelegateFactoryInterface"

Q_DECLARE_INTERFACE(iscore::SettingsDelegateFactoryInterface_QtInterface, SettingsDelegateFactoryInterface_QtInterface_iid)
