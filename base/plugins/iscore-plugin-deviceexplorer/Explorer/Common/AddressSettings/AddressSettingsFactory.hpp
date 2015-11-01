#pragma once
#include <QList>
#include <QString>
#include <QMap>
class AddressSettingsWidget;
class AddressSettingsWidgetFactoryMethod
{
    public:
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
            return m_instance;
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
