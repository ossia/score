// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressSettingsFactory.hpp"

#include <Explorer/Common/AddressSettings/Widgets/AddressBoolSettingsWidget.hpp>
#include <Explorer/Common/AddressSettings/Widgets/AddressCharSettingsWidget.hpp>
#include <Explorer/Common/AddressSettings/Widgets/AddressImpulseSettingsWidget.hpp>
#include <Explorer/Common/AddressSettings/Widgets/AddressListSettingsWidget.hpp>
#include <Explorer/Common/AddressSettings/Widgets/AddressNoneSettingsWidget.hpp>
#include <Explorer/Common/AddressSettings/Widgets/AddressNumericSettingsWidget.hpp>
#include <Explorer/Common/AddressSettings/Widgets/AddressStringSettingsWidget.hpp>
#include <Explorer/Common/AddressSettings/Widgets/AddressVecSettingsWidget.hpp>

namespace Explorer
{
using AddressIntSettingsWidget = AddressNumericSettingsWidget<int>;
using AddressFloatSettingsWidget = AddressNumericSettingsWidget<float>;

AddressSettingsWidget* AddressSettingsFactory::operator()(ossia::val_type valueType) const
{
  switch (valueType)
  {
    case ossia::val_type::NONE:
      return new AddressNoneSettingsWidget;
    case ossia::val_type::INT:
      return new AddressIntSettingsWidget;
    case ossia::val_type::FLOAT:
      return new AddressFloatSettingsWidget;
    case ossia::val_type::STRING:
      return new AddressStringSettingsWidget;
    case ossia::val_type::BOOL:
      return new AddressBoolSettingsWidget;
    case ossia::val_type::LIST:
      return new AddressListSettingsWidget;
    case ossia::val_type::CHAR:
      return new AddressCharSettingsWidget;
    case ossia::val_type::IMPULSE:
      return new AddressImpulseSettingsWidget;
    case ossia::val_type::VEC2F:
      return new AddressVecSettingsWidget<2>;
    case ossia::val_type::VEC3F:
      return new AddressVecSettingsWidget<3>;
    case ossia::val_type::VEC4F:
      return new AddressVecSettingsWidget<4>;
  }
  return nullptr;
}
}
