#pragma once
#include <QVariant>

namespace iscore
{
struct Value
{
        QVariant val;


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
};

using ValueList = QList<Value>;
}

Q_DECLARE_METATYPE(iscore::Value)
Q_DECLARE_METATYPE(iscore::ValueList)

