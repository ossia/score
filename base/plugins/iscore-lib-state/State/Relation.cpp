#include <State/ValueConversion.hpp>
#include <QMap>

#include "Relation.hpp"

namespace iscore {
struct Address;
struct Value;
}  // namespace iscore

QString iscore::Relation::relMemberToString(iscore::RelationMember m) const
{
    using namespace eggs::variants;
    switch(m.which())
    {
        case 0:
            return get<iscore::Address>(m).toString();
        case 1:
            return iscore::convert::toPrettyString(get<iscore::Value>(m));
        default:
            return "ERROR";
    }
}

QString iscore::Relation::toString() const
{
    using namespace eggs::variants;

    return QString("%1 %2 %3")
            .arg(relMemberToString(lhs))
            .arg(opToString()[op])
            .arg(relMemberToString(rhs));
}

const QMap<iscore::Relation::Operator, QString> iscore::opToString()
{
    return {
        {iscore::Relation::Operator::LowerEqual,    "<="},
        {iscore::Relation::Operator::GreaterEqual,  ">="},
        {iscore::Relation::Operator::Lower,         "<"},
        {iscore::Relation::Operator::Greater,       ">"},
        {iscore::Relation::Operator::Different,     "!="},
        {iscore::Relation::Operator::Equal,         "=="},
        {iscore::Relation::Operator::None,          ""}
    };
}
