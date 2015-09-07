#include "Value.hpp"

namespace
{
QString valueToString(const QVariant& val)
{
    switch(val.type())
    {
        case QMetaType::QVariantList:
        {
            QString s;
            s += "[ ";
            for(const auto& v : val.value<QVariantList>())
            {
                s += valueToString(v) + ", ";
            }
            s += " ]";
            return s;
        }
        case QMetaType::QChar:
        {
            return "'" + val.toString() + "'";
        }
        case QMetaType::QString:
        {
            return "\"" + val.toString() + "\"";
        }
        default:
        {
            return val.toString();
        }
    }
}
}

QString iscore::Value::toString() const
{
    return valueToString(val);
}
