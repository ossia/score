#pragma once

#include <eggs/variant/variant.hpp>
#include <QString>
#include <State/Address.hpp>
#include <State/Value.hpp>

namespace State
{
using RelationMember = eggs::variant<State::Address, State::Value>;

struct ISCORE_LIB_STATE_EXPORT Relation
{
        enum Operator {
            Equal,
            Different,
            Greater,
            Lower,
            GreaterEqual,
            LowerEqual,
            None
        } ;

        RelationMember lhs;
        Operator op;
        RelationMember rhs;

        friend bool operator==(const Relation& eq_lhs, const Relation& eq_rhs)
        {
            return eq_lhs.lhs == eq_rhs.lhs && eq_lhs.rhs == eq_rhs.rhs && eq_lhs.op == eq_rhs.op;
        }

        QString relMemberToString(RelationMember) const;
        QString toString() const;
};

ISCORE_LIB_STATE_EXPORT const QMap<State::Relation::Operator, QString> opToString();

}

using Comparator = State::Relation::Operator;
