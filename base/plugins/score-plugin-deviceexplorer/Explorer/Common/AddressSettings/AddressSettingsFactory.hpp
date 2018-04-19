#pragma once
#include <QList>
#include <QMap>
#include <QString>
#include <State/Value.hpp>
#include <memory>
namespace Explorer
{
class AddressSettingsWidget;

class AddressSettingsWidgetFactoryMethod
{
public:
  virtual ~AddressSettingsWidgetFactoryMethod();
  virtual AddressSettingsWidget* create() const
  {
    return nullptr;
  }
};

class AddressSettingsFactory
{
public:
  static AddressSettingsFactory& instance();

  AddressSettingsWidget* get_value_typeWidget(ossia::val_type valueType) const;

private:
  AddressSettingsFactory();

  std::
      map<ossia::val_type, std::unique_ptr<AddressSettingsWidgetFactoryMethod>>
          m_addressSettingsWidgetFactory;
};
}
