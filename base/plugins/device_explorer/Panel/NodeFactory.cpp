#include "NodeFactory.hpp"

#include <QDebug>

//protocols
#include "MinuitProtocolSettingsWidget.hpp"
#include "OSCProtocolSettingsWidget.hpp"
#include "MIDIProtocolSettingsWidget.hpp"

//value types
#include "AddressIntSettingsWidget.hpp"
#include "AddressFloatSettingsWidget.hpp"
#include "AddressStringSettingsWidget.hpp"



NodeFactory NodeFactory::m_instance;



template <typename T>
class ProtocolSettingsWidgetFactoryMethodT : public ProtocolSettingsWidgetFactoryMethod
{
    public:
        virtual ProtocolSettingsWidget* create() const
        {
            return new T;
        }
};


template <typename T>
class AddressSettingsWidgetFactoryMethodT : public AddressSettingsWidgetFactoryMethod
{
    public:
        virtual AddressSettingsWidget* create() const
        {
            return new T;
        }
};



template <typename T>
T* factoryMethod()
{
    return new T;
}


NodeFactory::NodeFactory()
{
    //TODO: retrieve from loaded plugins ?
    m_protocolSettingsWidgetFactory.insert (QObject::tr ("Minuit"), new ProtocolSettingsWidgetFactoryMethodT<MinuitProtocolSettingsWidget>);
    m_protocolSettingsWidgetFactory.insert (QObject::tr ("OSC"), new ProtocolSettingsWidgetFactoryMethodT<OSCProtocolSettingsWidget>);
    m_protocolSettingsWidgetFactory.insert (QObject::tr ("MIDI"), new ProtocolSettingsWidgetFactoryMethodT<MIDIProtocolSettingsWidget>);

    //TODO: retrieve from loaded plugins ?
    m_addressSettingsWidgetFactory.insert (QObject::tr ("Int"), new AddressSettingsWidgetFactoryMethodT<AddressIntSettingsWidget>);
    m_addressSettingsWidgetFactory.insert (QObject::tr ("Float"), new AddressSettingsWidgetFactoryMethodT<AddressFloatSettingsWidget>);
    m_addressSettingsWidgetFactory.insert (QObject::tr ("String"), new AddressSettingsWidgetFactoryMethodT<AddressStringSettingsWidget>);

}

QList<QString>
NodeFactory::getAvailableProtocols() const
{
    return m_protocolSettingsWidgetFactory.keys();
}

ProtocolSettingsWidget*
NodeFactory::getProtocolWidget (const QString& protocol) const
{
    ProtocolSettingsWidgetFactory::const_iterator it = m_protocolSettingsWidgetFactory.find (protocol);

    if (it != m_protocolSettingsWidgetFactory.end() )
    {
        return it.value()->create();
    }
    else
    {
        qDebug() << QObject::tr ("Unknown protocol: %1").arg (protocol) << "\n";
        return nullptr;
    }
}

QList<QString>
NodeFactory::getAvailableValueTypes() const
{
    return m_addressSettingsWidgetFactory.keys();
}

AddressSettingsWidget*
NodeFactory::getValueTypeWidget (const QString& valueType) const
{
    AddressSettingsWidgetFactory::const_iterator it = m_addressSettingsWidgetFactory.find (valueType);

    if (it != m_addressSettingsWidgetFactory.end() )
    {
        return it.value()->create();
    }
    else
    {
        return nullptr;
    }
}

QList<QString>
NodeFactory::getAvailableInputMIDIDevices() const
{
    //TODO: ask hardware...
    QList<QString> list;
    list.append (QObject::tr ("MIDI.1") );
    list.append (QObject::tr ("MIDI.5") );
    list.append (QObject::tr ("MIDI.Z") );
    return list;
}

QList<QString>
NodeFactory::getAvailableOutputMIDIDevices() const
{
    //TODO: ask hardware...
    QList<QString> list;
    list.append (QObject::tr ("MIDI.out.2") );
    list.append (QObject::tr ("MIDI.7") );
    list.append (QObject::tr ("MIDI.AB") );
    list.append (QObject::tr ("MIDI.Abcdef") );
    return list;
}


//TODO: put this elsewhere !!!

QList<QString> getIOTypes()
{
    QList<QString> list;
    list.append (QObject::tr ("In") );
    list.append (QObject::tr ("Out") );
    list.append (QObject::tr ("In/Out") );
    return list;
}

QList<QString> getUnits()
{
    QList<QString> list;
    list.append (QObject::tr ("--") );
    list.append (QObject::tr ("Hz") );
    list.append (QObject::tr ("dB") );
    list.append (QObject::tr ("s") );
    list.append (QObject::tr ("ms") );
    list.append (QObject::tr ("m") );
    list.append (QObject::tr ("mm") );
    return list;
}

QList<QString> getClipModes()
{
    QList<QString> list;
    list.append (QObject::tr ("none") );
    list.append (QObject::tr ("low") );
    list.append (QObject::tr ("high") );
    list.append (QObject::tr ("both") );
    return list;
}

#include <QComboBox>

void populateIOTypes (QComboBox* cbox)
{
    Q_ASSERT (cbox);
    cbox->addItems (getIOTypes() );
}

void populateUnit (QComboBox* cbox)
{
    Q_ASSERT (cbox);
    cbox->addItems (getUnits() );
}

void populateClipMode (QComboBox* cbox)
{
    Q_ASSERT (cbox);
    cbox->addItems (getClipModes() );
}
