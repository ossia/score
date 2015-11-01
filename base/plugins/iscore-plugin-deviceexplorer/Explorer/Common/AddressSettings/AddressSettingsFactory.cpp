#include "AddressSettingsFactory.hpp"

//value types
#include "Widgets/AddressNumericSettingsWidget.hpp"
#include "Widgets/AddressStringSettingsWidget.hpp"
#include "Widgets/AddressCharSettingsWidget.hpp"
#include "Widgets/AddressTupleSettingsWidget.hpp"
#include "Widgets/AddressBoolSettingsWidget.hpp"
#include "Widgets/AddressNoneSettingsWidget.hpp"


using AddressIntSettingsWidget = AddressNumericSettingsWidget<int>;
using AddressFloatSettingsWidget = AddressNumericSettingsWidget<float>;

template <typename T>
class AddressSettingsWidgetFactoryMethodT : public AddressSettingsWidgetFactoryMethod
{
    public:
        virtual AddressSettingsWidget* create() const override
        {
            return new T;
        }
};


AddressSettingsFactory AddressSettingsFactory::m_instance;

AddressSettingsFactory::AddressSettingsFactory()
{
    m_addressSettingsWidgetFactory.insert(QObject::tr("Int"),
                                          new AddressSettingsWidgetFactoryMethodT<AddressIntSettingsWidget>);
    m_addressSettingsWidgetFactory.insert(QObject::tr("Float"),
                                          new AddressSettingsWidgetFactoryMethodT<AddressFloatSettingsWidget>);
    m_addressSettingsWidgetFactory.insert(QObject::tr("String"),
                                          new AddressSettingsWidgetFactoryMethodT<AddressStringSettingsWidget>);
    m_addressSettingsWidgetFactory.insert(QObject::tr("Bool"),
                                          new AddressSettingsWidgetFactoryMethodT<AddressBoolSettingsWidget>);
    m_addressSettingsWidgetFactory.insert(QObject::tr("Tuple"),
                                          new AddressSettingsWidgetFactoryMethodT<AddressTupleSettingsWidget>);
    m_addressSettingsWidgetFactory.insert(QObject::tr("Char"),
                                          new AddressSettingsWidgetFactoryMethodT<AddressCharSettingsWidget>);
    m_addressSettingsWidgetFactory.insert(QObject::tr("Impulse"),
                                          new AddressSettingsWidgetFactoryMethodT<AddressNoneSettingsWidget>);
}



QList<QString>
AddressSettingsFactory::getAvailableValueTypes() const
{
    return m_addressSettingsWidgetFactory.keys();
}

AddressSettingsWidget*
AddressSettingsFactory::getValueTypeWidget(const QString& valueType) const
{
    auto it = m_addressSettingsWidgetFactory.find(valueType);

    if(it != m_addressSettingsWidgetFactory.end())
    {
        return it.value()->create();
    }
    else
    {
        return nullptr;
    }
}
