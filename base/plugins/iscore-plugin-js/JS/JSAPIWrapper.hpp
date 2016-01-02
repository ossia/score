#pragma once
#include <Process/TimeValue.hpp>
#include <State/Message.hpp>
#include <eggs/variant/variant.hpp>
#include <QChar>
#include <QJSEngine>
#include <QJSValue>
#include <QJSValueIterator>
#include <QObject>
#include <QString>

#include <State/Address.hpp>
#include <State/Value.hpp>

// TODO cleanup this file

namespace iscore
{
namespace convert
{
namespace JS
{
inline QJSValue makeImpulse(
        QJSEngine& engine)
{
    auto obj = engine.newObject();
    obj.setProperty("impulse", 1);
    return obj;
}

inline QJSValue value(
        QJSEngine& engine,
        const State::Value& val)
{
    static const struct {
            QJSEngine& engine;

        public:
            using return_type = QJSValue;
            return_type operator()(const State::no_value_t&) const { return {}; }
            return_type operator()(const State::impulse_t&) const {
                return makeImpulse(engine);
            }
            return_type operator()(int i) const { return i; }
            return_type operator()(float f) const { return f; }
            return_type operator()(bool b) const { return b; }
            return_type operator()(const QString& s) const { return s; }

            return_type operator()(const QChar& c) const
            {
                // Note : it is saved as a string but the actual type should be saved also
                // so that the QChar can be recovered.
                return QString(c);
            }

            return_type operator()(const State::tuple_t& t) const
            {
                auto arr = engine.newArray(t.size());

                int i = 0;
                for(const auto& elt : t)
                {
                    arr.setProperty(i++, eggs::variants::apply(*this, elt.impl()));
                }

                return arr;
            }
    } visitor{engine};

    return eggs::variants::apply(visitor, val.val.impl());
}

inline QJSValue address(
        const State::Address& val)
{
    return val.toString();
}

inline QJSValue message(
        QJSEngine& engine,
        const State::Message& mess)
{
    auto obj = engine.newObject();
    obj.setProperty("address", address(mess.address));
    obj.setProperty("value", value(engine, mess.value));
    return obj;
}

inline QJSValue time(const TimeValue& val)
{
    return val.msec();
}

// TODO vector instead of MessageList.
inline QJSValue messages(
        QJSEngine& engine,
        const State::MessageList& messages)
{
    auto obj = engine.newArray(messages.size());
    for(int i = 0; i < messages.size(); i++)
    {
        obj.setProperty(i, message(engine, messages.at(i)));
    }
    return obj;
}

}
}
}

namespace JS
{
namespace convert
{

inline State::ValueImpl value(const QJSValue& val)
{
    if(val.isUndefined() || val.isNull() || val.isError())
        return {};

    if(val.isBool())
        return val.toBool();

    if(val.isNumber())
        return val.toNumber();

    if(val.isString())
        return val.toString();

    if(val.isObject())
    {
        if(val.hasProperty("impulse"))
        {
            return State::impulse_t{};
        }
    }

    if(val.isArray())
    {
        State::tuple_t arr;

        QJSValueIterator it{val};
        while(it.hasNext())
        {
            it.next();
            arr.push_back(value(it.value()));
        }
        return arr;
    }

    return {};
}

inline State::Message message(const QJSValue& val)
{
    if(val.isObject())
    {
        auto iscore_addr = val.property("address");
        auto iscore_val = val.property("value");
        if(iscore_addr.isString())
        {
            return {State::Address::fromString(iscore_addr.toString()), value(iscore_val)};
        }
    }

    return {};
}

inline State::MessageList messages(const QJSValue& val)
{
    if(!val.isArray())
        return {};

    State::MessageList ml;
    QJSValueIterator it{val};
    while(it.hasNext())
    {
        it.next();
        auto mess = message(it.value());
        if(!mess.address.device.isEmpty())
        {
            ml.append(mess);
        }

    }

    return ml;
}

}
}

class DeviceDocumentPlugin;

namespace JS
{
class APIWrapper : public QObject
{
        Q_OBJECT
    public:
        APIWrapper(DeviceDocumentPlugin& devs):
            devices{devs}
        {

        }

    public slots:
        QJSValue value(QJSValue address);
    private:
        DeviceDocumentPlugin& devices;

};
}
