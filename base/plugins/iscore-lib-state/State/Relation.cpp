#include <State/ValueConversion.hpp>
#include <QMap>

#include "Relation.hpp"

namespace State {
struct Address;
struct Value;
}  // namespace iscore

QString State::Relation::relMemberToString(State::RelationMember m) const
{
    using namespace eggs::variants;
    switch(m.which())
    {
        case 0:
            return get<State::Address>(m).toString();
        case 1:
            return State::convert::toPrettyString(get<State::Value>(m));
        default:
            return "ERROR";
    }
}

QString State::Relation::toString() const
{
    using namespace eggs::variants;

    return QString("%1 %2 %3")
            .arg(relMemberToString(lhs))
            .arg(opToString()[op])
            .arg(relMemberToString(rhs));
}

const QMap<State::Relation::Operator, QString> State::opToString()
{
    return {
        {State::Relation::Operator::LowerEqual,    "<="},
        {State::Relation::Operator::GreaterEqual,  ">="},
        {State::Relation::Operator::Lower,         "<"},
        {State::Relation::Operator::Greater,       ">"},
        {State::Relation::Operator::Different,     "!="},
        {State::Relation::Operator::Equal,         "=="},
        {State::Relation::Operator::None,          ""}
    };
}
