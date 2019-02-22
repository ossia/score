#pragma once
#include <State/Value.hpp>

namespace Explorer
{
class AddressSettingsWidget;
class AddressSettingsFactory
{
public:
  AddressSettingsWidget* operator()(ossia::val_type valueType) const;
};
}
