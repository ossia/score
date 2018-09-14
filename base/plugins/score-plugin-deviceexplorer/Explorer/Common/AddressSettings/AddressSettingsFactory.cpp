// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressSettingsFactory.hpp"

#include "Widgets/AddressBoolSettingsWidget.hpp"
#include "Widgets/AddressCharSettingsWidget.hpp"
#include "Widgets/AddressImpulseSettingsWidget.hpp"
#include "Widgets/AddressListSettingsWidget.hpp"
#include "Widgets/AddressNoneSettingsWidget.hpp"
#include "Widgets/AddressNumericSettingsWidget.hpp"
#include "Widgets/AddressStringSettingsWidget.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Explorer/Common/AddressSettings/Widgets/AddressVecSettingsWidget.hpp>

#include <QObject>

namespace Explorer
{
using AddressIntSettingsWidget = AddressNumericSettingsWidget<int>;
using AddressFloatSettingsWidget = AddressNumericSettingsWidget<float>;

template <typename T>
class AddressSettingsWidgetFactoryMethodT final
    : public AddressSettingsWidgetFactoryMethod
{
public:
  virtual ~AddressSettingsWidgetFactoryMethodT() = default;

  AddressSettingsWidget* create() const override
  {
    return new T;
  }
};

AddressSettingsFactory::AddressSettingsFactory()
{
  m_addressSettingsWidgetFactory.emplace(std::make_pair(
      ossia::val_type::NONE,
      std::make_unique<
          AddressSettingsWidgetFactoryMethodT<AddressNoneSettingsWidget>>()));
  m_addressSettingsWidgetFactory.emplace(std::make_pair(
      ossia::val_type::INT,
      std::make_unique<
          AddressSettingsWidgetFactoryMethodT<AddressIntSettingsWidget>>()));
  m_addressSettingsWidgetFactory.emplace(std::make_pair(
      ossia::val_type::FLOAT,
      std::make_unique<
          AddressSettingsWidgetFactoryMethodT<AddressFloatSettingsWidget>>()));
  m_addressSettingsWidgetFactory.emplace(std::make_pair(
      ossia::val_type::STRING,
      std::make_unique<AddressSettingsWidgetFactoryMethodT<
          AddressStringSettingsWidget>>()));
  m_addressSettingsWidgetFactory.emplace(std::make_pair(
      ossia::val_type::BOOL,
      std::make_unique<
          AddressSettingsWidgetFactoryMethodT<AddressBoolSettingsWidget>>()));
  m_addressSettingsWidgetFactory.emplace(std::make_pair(
      ossia::val_type::LIST,
      std::make_unique<
          AddressSettingsWidgetFactoryMethodT<AddressListSettingsWidget>>()));
  m_addressSettingsWidgetFactory.emplace(std::make_pair(
      ossia::val_type::CHAR,
      std::make_unique<
          AddressSettingsWidgetFactoryMethodT<AddressCharSettingsWidget>>()));
  m_addressSettingsWidgetFactory.emplace(std::make_pair(
      ossia::val_type::IMPULSE,
      std::make_unique<AddressSettingsWidgetFactoryMethodT<
          AddressImpulseSettingsWidget>>()));

  m_addressSettingsWidgetFactory.emplace(std::make_pair(
      ossia::val_type::VEC2F,
      std::make_unique<AddressSettingsWidgetFactoryMethodT<
          AddressVecSettingsWidget<2>>>()));
  m_addressSettingsWidgetFactory.emplace(std::make_pair(
      ossia::val_type::VEC3F,
      std::make_unique<AddressSettingsWidgetFactoryMethodT<
          AddressVecSettingsWidget<3>>>()));
  m_addressSettingsWidgetFactory.emplace(std::make_pair(
      ossia::val_type::VEC4F,
      std::make_unique<AddressSettingsWidgetFactoryMethodT<
          AddressVecSettingsWidget<4>>>()));
}

AddressSettingsFactory& AddressSettingsFactory::instance()
{
  static AddressSettingsFactory inst;
  return inst; // FIXME Blergh
}

AddressSettingsWidget*
AddressSettingsFactory::get_value_typeWidget(ossia::val_type valueType) const
{
  auto it = m_addressSettingsWidgetFactory.find(valueType);

  if (it != m_addressSettingsWidgetFactory.end())
  {
    return it->second->create();
  }
  else
  {
    return nullptr;
  }
}

AddressSettingsWidgetFactoryMethod::~AddressSettingsWidgetFactoryMethod()
{
}
}
