#pragma once
#include <QVariant>
#include <iscore/tools/Todo.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <boost/optional.hpp>
#include <eggs/variant.hpp>


namespace iscore
{
struct impulse_t {};
class ValueImpl;
using tuple_t = std::vector<ValueImpl>;

class ValueImpl
{
    public:
        using variant_t = eggs::variant<impulse_t, int, float, bool, QString, QChar, tuple_t>;
        ValueImpl() = default;
        ValueImpl(const ValueImpl&) = default;
        ValueImpl(ValueImpl&&) = default;

        ValueImpl& operator=(const ValueImpl&) = default;
        ValueImpl& operator=(ValueImpl&&) = default;

        ValueImpl(impulse_t v): m_variant{v} { }
        ValueImpl(int v): m_variant{v} { }
        ValueImpl(float v): m_variant{v} { }
        ValueImpl(double v): m_variant{(float)v} { }
        ValueImpl(bool v): m_variant{v} { }
        ValueImpl(const QString& v): m_variant{v} { }
        ValueImpl(QChar v): m_variant{v} { }
        ValueImpl(const tuple_t& v): m_variant{v} { }

        ValueImpl& operator=(impulse_t v) { m_variant = v; return *this; }
        ValueImpl& operator=(int v) { m_variant = v; return *this;  }
        ValueImpl& operator=(float v) { m_variant = v; return *this;  }
        ValueImpl& operator=(double v) { m_variant = (float)v; return *this;  }
        ValueImpl& operator=(bool v) { m_variant = v; return *this;  }
        ValueImpl& operator=(const QString& v) { m_variant = v; return *this;  }
        ValueImpl& operator=(QChar v) { m_variant = v; return *this;  }
        ValueImpl& operator=(const tuple_t& v) { m_variant = v; return *this;  }

        const auto& impl() const
        { return m_variant; }

        const char* typeName() const { return ""; }
        int type() const { return -1; }
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

    private:
        variant_t m_variant;
};
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
            return iscore::Value{value_type{std::forward<Val>(val)}};
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

        static iscore::Value fromQVariant(const QVariant& var);
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

