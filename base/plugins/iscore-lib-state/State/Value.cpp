#include "Value.hpp"

#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

namespace
{

class VariantToString final : public boost::static_visitor<QString>
{
    public:
        QString operator()(const impulse_t&) const { return {}; }
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

        QString operator()(const tuple_t& t) const
        {
            QString s{"["};

            for(const auto& elt : t)
            {
                s += boost::apply_visitor(*this, elt);
                s += ", ";
            }

            s+= "]";
            return s;
        }
};
}

QString iscore::Value::toString() const
{
    return boost::apply_visitor(VariantToString{}, this->val);
}
