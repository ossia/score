#pragma once

#include <QList>
#include <QMap>
#include <QString>

class ProtocolSettingsWidget;
class AddressSettingsWidget;


class ProtocolSettingsWidgetFactoryMethod
{
    public:
        virtual ProtocolSettingsWidget* create() const
        {
            return nullptr;
        }
};

class AddressSettingsWidgetFactoryMethod
{
    public:
        virtual AddressSettingsWidget* create() const
        {
            return nullptr;
        }
};


class NodeFactory
{
    public:

        static NodeFactory& instance()
        {
            return m_instance;
        }


        QList<QString> getAvailableProtocols() const;

        ProtocolSettingsWidget* getProtocolWidget (const QString& protocol) const;

        QList<QString> getAvailableValueTypes() const;

        AddressSettingsWidget* getValueTypeWidget (const QString& valueType) const;

        QList<QString> getAvailableInputMIDIDevices() const;
        QList<QString> getAvailableOutputMIDIDevices() const;

    private:

        NodeFactory();

    private:

        static NodeFactory m_instance;

        typedef ProtocolSettingsWidget* (ProtocolSettingsWidgetFactoryM) ();
        typedef QMap<QString, ProtocolSettingsWidgetFactoryMethod*> ProtocolSettingsWidgetFactory;
        ProtocolSettingsWidgetFactory m_protocolSettingsWidgetFactory;

        typedef AddressSettingsWidget* (AddressSettingsWidgetFactoryM) ();
        typedef QMap<QString, AddressSettingsWidgetFactoryMethod*> AddressSettingsWidgetFactory;
        AddressSettingsWidgetFactory m_addressSettingsWidgetFactory;


};


//TODO: put this elsewhere !!!

class QComboBox;

extern QList<QString> getIOTypes();

extern QList<QString> getUnits();

extern QList<QString> getClipModes();

extern void populateIOTypes (QComboBox* cbox);

extern void populateUnit (QComboBox* cbox);

extern void populateClipMode (QComboBox* cbox);

