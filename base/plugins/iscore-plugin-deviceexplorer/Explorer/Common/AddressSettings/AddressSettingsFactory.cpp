#include <QObject>


#include "AddressSettingsFactory.hpp"
#include <Device/Address/AddressSettings.hpp>
#include "Widgets/AddressBoolSettingsWidget.hpp"
#include "Widgets/AddressCharSettingsWidget.hpp"
#include "Widgets/AddressNoneSettingsWidget.hpp"
#include "Widgets/AddressImpulseSettingsWidget.hpp"
#include "Widgets/AddressNumericSettingsWidget.hpp"
#include "Widgets/AddressStringSettingsWidget.hpp"
#include "Widgets/AddressTupleSettingsWidget.hpp"


namespace Explorer
{
using AddressIntSettingsWidget = AddressNumericSettingsWidget<int>;
using AddressFloatSettingsWidget = AddressNumericSettingsWidget<float>;

template <typename T>
class AddressSettingsWidgetFactoryMethodT final : public AddressSettingsWidgetFactoryMethod
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
    // TODO important these strings must be the same than ValueTypesArray in ValueConversion.
    // Change this by storing keys instead (or the ValueType enum).

    m_addressSettingsWidgetFactory.emplace(std::make_pair(State::ValueType::NoValue,
                                          std::make_unique< AddressSettingsWidgetFactoryMethodT<AddressNoneSettingsWidget> >()));
    m_addressSettingsWidgetFactory.emplace(std::make_pair(State::ValueType::Int,
                                          std::make_unique< AddressSettingsWidgetFactoryMethodT<AddressIntSettingsWidget> >()));
    m_addressSettingsWidgetFactory.emplace(std::make_pair(State::ValueType::Float,
                                          std::make_unique< AddressSettingsWidgetFactoryMethodT<AddressFloatSettingsWidget> >()));
    m_addressSettingsWidgetFactory.emplace(std::make_pair(State::ValueType::String,
                                          std::make_unique< AddressSettingsWidgetFactoryMethodT<AddressStringSettingsWidget> >()));
    m_addressSettingsWidgetFactory.emplace(std::make_pair(State::ValueType::Bool,
                                          std::make_unique< AddressSettingsWidgetFactoryMethodT<AddressBoolSettingsWidget> >()));
    m_addressSettingsWidgetFactory.emplace(std::make_pair(State::ValueType::Tuple,
                                          std::make_unique< AddressSettingsWidgetFactoryMethodT<AddressTupleSettingsWidget> >()));
    m_addressSettingsWidgetFactory.emplace(std::make_pair(State::ValueType::Char,
                                          std::make_unique< AddressSettingsWidgetFactoryMethodT<AddressCharSettingsWidget> >()));
    m_addressSettingsWidgetFactory.emplace(std::make_pair(State::ValueType::Impulse,
                                          std::make_unique< AddressSettingsWidgetFactoryMethodT<AddressImpulseSettingsWidget> >()));
}



AddressSettingsFactory&AddressSettingsFactory::instance()
{
    static AddressSettingsFactory inst;
    return inst; // FIXME Blergh
}

AddressSettingsWidget*
AddressSettingsFactory::getValueTypeWidget(State::ValueType valueType) const
{
    auto it = m_addressSettingsWidgetFactory.find(valueType);

    if(it != m_addressSettingsWidgetFactory.end())
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
