#include "ValueConversion.hpp"
#include <QJsonArray>
template<>
QVariant iscore::convert::value(const iscore::Value& val)
{
    ISCORE_TODO;
}

template<>
QJsonValue iscore::convert::value(const iscore::Value& val)
{
    static const constexpr struct {
        public:
            using return_type = QJsonValue;
            return_type operator()(const impulse_t&) const { return {}; }
            return_type operator()(int i) const { return i; }
            return_type operator()(float f) const { return f; }
            return_type operator()(bool b) const { return b; }
            return_type operator()(const QString& s) const { return s; }

            return_type operator()(const QChar& c) const
            {
                // TODO check this
                return QString(c);
            }

            return_type operator()(const tuple_t& t) const
            {
                QJsonArray arr;

                for(const auto& elt : t)
                {
                    arr.append(eggs::variants::apply(*this, elt.impl()));
                }

                return arr;
            }
    } visitor{};

    return eggs::variants::apply(visitor, val.val.impl());
}


QString iscore::convert::textualType(const iscore::Value& val)
{
    // TODO apply_type ?
    static const constexpr struct {
        public:
            using return_type = QString;
            return_type operator()(const impulse_t&) const { return "Impulse"; }
            return_type operator()(int) const { return "Int"; }
            return_type operator()(float) const { return "Float"; }
            return_type operator()(bool) const { return "Bool"; }
            return_type operator()(const QString&) const { return "String"; }
            return_type operator()(const QChar&) const { return "Char"; }
            return_type operator()(const tuple_t&) const { return "Tuple"; }
    } visitor{};

    return eggs::variants::apply(visitor, val.val.impl());

}

#include <array>

static std::size_t which(const QString& val)
{
    static const std::array<QString, 7> types{{
        QString{"Impulse"}, QString{"Int"}, QString{"Float"}, QString{"Bool"}, QString{"String"}, QString{"Char"}, QString{"Tuple"}
    }};

    auto it = std::find(types.begin(), types.end(), val);
    if(it != types.end())
        return std::distance(types.begin(), it);
    return iscore::ValueImpl::variant_t::npos;
}


static iscore::Value toValue(const QJsonValue& val, std::size_t which)
{
}

iscore::Value iscore::convert::toValue(const QJsonValue& val, const QString& type)
{
    return toValue(val, which(type));
}

