#include "NodeFactory.hpp"

#include <QDebug>

//protocols
#include <DeviceExplorer/Protocol/MIDI/MIDIProtocolSettingsWidget.hpp>
#include <DeviceExplorer/Protocol/OSC/OSCProtocolSettingsWidget.hpp>
#include <DeviceExplorer/Protocol/Minuit/MinuitProtocolSettingsWidget.hpp>


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
T* factoryMethod()
{
    return new T;
}


NodeFactory::NodeFactory()
{
    //TODO: retrieve from loaded plugins ?
    m_protocolSettingsWidgetFactory.insert(QObject::tr("Minuit"), new ProtocolSettingsWidgetFactoryMethodT<MinuitProtocolSettingsWidget>);
    m_protocolSettingsWidgetFactory.insert(QObject::tr("OSC"), new ProtocolSettingsWidgetFactoryMethodT<OSCProtocolSettingsWidget>);
    m_protocolSettingsWidgetFactory.insert(QObject::tr("MIDI"), new ProtocolSettingsWidgetFactoryMethodT<MIDIProtocolSettingsWidget>);

}

QList<QString>
NodeFactory::getAvailableProtocols() const
{
    return m_protocolSettingsWidgetFactory.keys();
}

ProtocolSettingsWidget*
NodeFactory::getProtocolWidget(const QString& protocol) const
{
    ProtocolSettingsWidgetFactory::const_iterator it = m_protocolSettingsWidgetFactory.find(protocol);

    if(it != m_protocolSettingsWidgetFactory.end())
    {
        return it.value()->create();
    }
    else
    {
        qDebug() << QObject::tr("Unknown protocol: %1").arg(protocol) << "\n";
        return nullptr;
    }
}

QList<QString>
NodeFactory::getAvailableInputMIDIDevices() const
{
    //TODO: ask hardware...
    QList<QString> list;
    list.append(QObject::tr("MIDI.1"));
    list.append(QObject::tr("MIDI.5"));
    list.append(QObject::tr("MIDI.Z"));
    return list;
}

QList<QString>
NodeFactory::getAvailableOutputMIDIDevices() const
{
    //TODO: ask hardware...
    QList<QString> list;
    list.append(QObject::tr("MIDI.out.2"));
    list.append(QObject::tr("MIDI.7"));
    list.append(QObject::tr("MIDI.AB"));
    list.append(QObject::tr("MIDI.Abcdef"));
    return list;
}

