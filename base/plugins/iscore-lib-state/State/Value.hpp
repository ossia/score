#pragma once
#include <QVariant>
#include <iscore/serialization/VisitorInterface.hpp>
#include <boost/optional.hpp>

namespace iscore
{
/**
 * @brief The Value struct
 *
 * A variant used to represent the data that can be in a message.
 *
 */
struct Value
{
        QVariant val{};

        friend QDataStream& operator<<(QDataStream& s, const Value& m)
        {
            s << m.val;
            return s;
        }

        friend QDataStream& operator>>(QDataStream& s, Value& m)
        {
            s >> m.val;
            return s;
        }

        static Value fromVariant(const QVariant& var)
        {
            Value v;
            v.val = var;
            return v;
        }

        Value() = default;
        Value(const Value&) = default;
        Value(Value&&) = default;
        Value& operator=(const Value&) = default;
        Value& operator=(Value&&) = default;

        bool operator==(const Value& m) const
        {
            return val == m.val;
        }

        bool operator!=(const Value& m) const
        {
            return val != m.val;
        }

        bool operator<(const Value& m) const
        {
            return false;
        }

        QString toString() const;
        /*
        template<typename Fun> auto map(Fun&& f)
        {
            // todo make a generic switch like serialize_dyn.
        }
        */
};

using ValueList = QList<Value>;
using OptionalValue = boost::optional<iscore::Value>;
}

Q_DECLARE_METATYPE(iscore::Value)
Q_DECLARE_METATYPE(iscore::ValueList)

