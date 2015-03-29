#include "AddressSettingsFactory.hpp"

//value types
#include "Widgets/AddressIntSettingsWidget.hpp"
#include "Widgets/AddressFloatSettingsWidget.hpp"
#include "Widgets/AddressStringSettingsWidget.hpp"



template <typename T>
class AddressSettingsWidgetFactoryMethodT : public AddressSettingsWidgetFactoryMethod
{
    public:
        virtual AddressSettingsWidget* create() const
        {
            return new T;
        }
};


AddressSettingsFactory AddressSettingsFactory::m_instance;

AddressSettingsFactory::AddressSettingsFactory()
{
    //TODO: retrieve from loaded plugins ?
    m_addressSettingsWidgetFactory.insert(QObject::tr("Int"), new AddressSettingsWidgetFactoryMethodT<AddressIntSettingsWidget>);
    m_addressSettingsWidgetFactory.insert(QObject::tr("Float"), new AddressSettingsWidgetFactoryMethodT<AddressFloatSettingsWidget>);
    m_addressSettingsWidgetFactory.insert(QObject::tr("String"), new AddressSettingsWidgetFactoryMethodT<AddressStringSettingsWidget>);
}



QList<QString>
AddressSettingsFactory::getAvailableValueTypes() const
{
    return m_addressSettingsWidgetFactory.keys();
}

AddressSettingsWidget*
AddressSettingsFactory::getValueTypeWidget(const QString& valueType) const
{
    AddressSettingsWidgetFactory::const_iterator it = m_addressSettingsWidgetFactory.find(valueType);

    if(it != m_addressSettingsWidgetFactory.end())
    {
        return it.value()->create();
    }
    else
    {
        return nullptr;
    }
}
