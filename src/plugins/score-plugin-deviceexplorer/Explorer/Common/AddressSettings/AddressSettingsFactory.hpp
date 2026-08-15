#pragma once
#include <State/Value.hpp>

#include <score_plugin_deviceexplorer_export.h>

namespace Explorer
{
class AddressSettingsWidget;
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT AddressSettingsFactory
{
public:
  AddressSettingsWidget* operator()(ossia::val_type valueType) const;
};
}
