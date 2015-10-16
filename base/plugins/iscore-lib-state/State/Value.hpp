#pragma once
#include <QVariant>
#include <iscore/tools/Todo.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <boost/optional.hpp>
#include <boost/variant.hpp>
#include <eggs/variant.hpp>

struct impulse_t {};
class ValueImpl;
using tuple_t = std::vector<ValueImpl>;

class ValueImpl : public boost::variant<impulse_t, int, float, bool, QString, QChar, tuple_t>
{
    public:
        using variant_t = boost::variant<impulse_t, int, float, bool, QString, QChar, tuple_t>;

        ValueImpl() = default;
        ValueImpl(ValueImpl&& other): variant_t{std::move(static_cast<const variant_t&&>(other))} {}
        ValueImpl(const ValueImpl& other): variant_t{static_cast<const variant_t&>(other)} {}
        ValueImpl(const QVariant&) { ISCORE_TODO; }
        const char* typeName() const { return ""; }
        int type() const { return -1; }
        ValueImpl& operator =(const QVariant& other) { return *this; }
        ValueImpl& operator =(const ValueImpl& other) { return *this; }
        bool operator ==(const ValueImpl& other) const { return true; }
        bool operator !=(const ValueImpl& other) const { return true; }

        bool isValid() const { return {}; }
        int toInt() const { return {}; }
        float toFloat() const { return {}; }
        bool toBool() const { return {}; }
        QString toString() const { return {}; }
        QStringList toStringList() const { return {}; }
        QChar toChar() const { return {}; }
        tuple_t toTuple() const { return {}; }

        template<typename TheType>
        bool canConvert() const
        { return {}; }

        template<typename TheType>
        TheType value() const
        { return {}; }


        friend QDebug& operator<<(QDebug& s, const ValueImpl& m)
        {
            return s;
        }
        friend QDataStream& operator<<(QDataStream& s, const ValueImpl& m)
        {
            return s;
        }

        friend QDataStream& operator>>(QDataStream& s, ValueImpl& m)
        {
            return s;
        }
};

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
        using value_type = ValueImpl;
        value_type val{};

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

        template<typename Val>
        static Value fromValue(Val&& val)
        {
            return iscore::Value{value_type{val}};
        }

        static Value fromVariant(const value_type& var)
        {
            return iscore::Value{var};
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

        QVariant toQVariant() const;
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

