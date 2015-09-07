#include "Relation.hpp"


QString iscore::Relation::toString() const
{
    using namespace eggs::variants;
    static const auto relMemberToString = [] (const auto& m) -> QString
    {
        switch(m.which())
        {
            case 0:
                return get<iscore::Address>(m).toString();
            case 1:
                return get<iscore::Value>(m).toString();
            default:
                return "ERROR";
        }
    };

    // todo boost.bimap
    static const QMap<iscore::Relation::Operator, QString> opToString
    {
        {iscore::Relation::Operator::LowerEqual,    "<="},
        {iscore::Relation::Operator::GreaterEqual,  ">="},
        {iscore::Relation::Operator::Lower,         "<"},
        {iscore::Relation::Operator::Greater,       ">"},
        {iscore::Relation::Operator::Different,     "!="},
        {iscore::Relation::Operator::Equal,         "=="}
    };

    return QString("%1 %2 %3")
            .arg(relMemberToString(lhs))
            .arg(opToString[op])
            .arg(relMemberToString(rhs));
}
