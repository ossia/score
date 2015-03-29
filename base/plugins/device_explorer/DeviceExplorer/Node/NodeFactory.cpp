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


NodeFactory::NodeFactory()
{
    //TODO: retrieve from loaded plugins ?
    m_protocolSettingsWidgetFactory.insert(
                QObject::tr("Minuit"),
                new ProtocolSettingsWidgetFactoryMethodT<MinuitProtocolSettingsWidget>);
    m_protocolSettingsWidgetFactory.insert(
                QObject::tr("OSC"),
                new ProtocolSettingsWidgetFactoryMethodT<OSCProtocolSettingsWidget>);
    m_protocolSettingsWidgetFactory.insert(
                QObject::tr("MIDI"),
                new ProtocolSettingsWidgetFactoryMethodT<MIDIProtocolSettingsWidget>);

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
