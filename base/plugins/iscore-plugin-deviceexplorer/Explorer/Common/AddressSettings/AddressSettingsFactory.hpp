#pragma once
#include <QList>
#include <QMap>
#include <QString>

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
        static AddressSettingsFactory& instance()
        {
            return m_instance; // FIXME Blergh
        }

        QList<QString> getAvailableValueTypes() const;
        AddressSettingsWidget* getValueTypeWidget(const QString& valueType) const;

    private:
        static AddressSettingsFactory m_instance;

        AddressSettingsFactory();
        typedef AddressSettingsWidget* (AddressSettingsWidgetFactoryM)();
        typedef QMap<QString, AddressSettingsWidgetFactoryMethod*> AddressSettingsWidgetFactory;
        AddressSettingsWidgetFactory m_addressSettingsWidgetFactory;
};
}
