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

class NodeFactory
{
    public:
        static NodeFactory& instance()
        {
            return m_instance;
        }

        QList<QString> getAvailableProtocols() const;
        ProtocolSettingsWidget* getProtocolWidget(const QString& protocol) const;

        QList<QString> getAvailableInputMIDIDevices() const;
        QList<QString> getAvailableOutputMIDIDevices() const;

    private:
        NodeFactory();

    private:
        static NodeFactory m_instance;

        typedef ProtocolSettingsWidget* (ProtocolSettingsWidgetFactoryM)();
        typedef QMap<QString, ProtocolSettingsWidgetFactoryMethod*> ProtocolSettingsWidgetFactory;
        ProtocolSettingsWidgetFactory m_protocolSettingsWidgetFactory;
};
