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

  AddressSettingsWidget* get_value_typeWidget(State::ValueType valueType) const;

private:
  AddressSettingsFactory();

  std::
      map<State::ValueType,
          std::unique_ptr<AddressSettingsWidgetFactoryMethod>>
          m_addressSettingsWidgetFactory;
};
}
