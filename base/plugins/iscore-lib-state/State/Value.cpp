#include "Value.hpp"


namespace
{

class VariantToString
{
    public:
        QString operator()(const iscore::impulse_t&) const { return {}; }
        QString operator()(int i) const { return QString::number(i); }
        QString operator()(float f) const { return QString::number(f); }
        QString operator()(bool b) const {
            static const QString tr = "true";
            static const QString f = "false";
            return b ? tr : f;
        }
        QString operator()(const QString& s) const
        {
            // TODO escape ?
            return QString("\"%1\"").arg(s);
        }

        QString operator()(const QChar& c) const
        {
            return QString("'%1'").arg(c);
        }

        QString operator()(const iscore::tuple_t& t) const
        {
            QString s{"["};

            for(const auto& elt : t)
            {
                s += eggs::variants::apply(*this, elt.impl());
                s += ", ";
            }

            s+= "]";
            return s;
        }
};
}

iscore::Value iscore::Value::fromQVariant(const QVariant& var)
{
    ISCORE_TODO;
    return {};
}

QVariant iscore::Value::toQVariant() const
{
    ISCORE_TODO;
    return {};
}

QString iscore::Value::toString() const
{
    return eggs::variants::apply(VariantToString{}, val.impl());
}
