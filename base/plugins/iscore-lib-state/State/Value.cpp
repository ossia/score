#include "Value.hpp"

#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>


struct impulse_t {};
#include <eggs/variant.hpp>
class Val;
using Tuple = std::vector<Val>;
using Variant = eggs::variant<int, std::string, Tuple>;
class Val : public Variant
{
    public:
        using Variant::variant;
};

class ToStr final : public boost::static_visitor<std::string>
{
    public:

        std::string operator()(int i) const { return ""; }

        std::string operator()(const std::string& s) const
        {
            return s;
        }

        std::string operator()(const Tuple& t) const
        {
            std::string s;

            for(const auto& elt : t)
            {
                s += boost::apply_visitor(*this, elt);
            }
            return s;
        }
};

namespace
{

class VariantToString final : public boost::static_visitor<QString>
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
                s += boost::apply_visitor(*this, elt.impl());
                s += ", ";
            }

            s+= "]";
            return s;
        }
};
}

QVariant iscore::Value::toQVariant() const
{
    ISCORE_TODO;
    return {};
}

QString iscore::Value::toString() const
{
    return boost::apply_visitor(VariantToString{}, this->val);
}
