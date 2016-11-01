#pragma once
#include <Process/TimeValue.hpp>
#include <State/Message.hpp>
#include <eggs/variant/variant.hpp>
#include <QChar>
#include <QtQml/QJSEngine>
#include <QtQml/QJSValue>
#include <QtQml/QJSValueIterator>
#include <QObject>
#include <QString>

#include <State/Address.hpp>
#include <State/Value.hpp>

// TODO cleanup this file

namespace Explorer
{
class DeviceDocumentPlugin;
}

namespace JS
{
namespace convert
{
QJSValue makeImpulse(QJSEngine& engine);

QJSValue value(
        QJSEngine& engine,
        const State::Value& val);

QJSValue address(const State::AddressAccessor& val);

QJSValue message(
        QJSEngine& engine,
        const State::Message& mess);

QJSValue time(const TimeValue& val);

// TODO vector instead of MessageList.
QJSValue messages(
        QJSEngine& engine,
        const State::MessageList& messages);

State::ValueImpl value(const QJSValue& val);
State::Message message(const QJSValue& val);
State::MessageList messages(const QJSValue& val);

}

class APIWrapper : public QObject
{
        Q_OBJECT
    public:
        APIWrapper(
                QJSEngine& engine,
                const Explorer::DeviceDocumentPlugin& devs):
            m_engine{engine},
            devices{devs}
        {

        }

    public slots:
        QJSValue value(QJSValue address);

    private:
        QJSEngine& m_engine;
        const Explorer::DeviceDocumentPlugin& devices;

};
}
