#pragma once
#include <State/Address.hpp>
#include <State/Value.hpp>
#include <eggs/variant.hpp>

namespace iscore
{
using RelationMember = eggs::variant<iscore::Address, iscore::Value>;

struct Relation
{
        enum Operator {
            Equal,
            Different,
            Greater,
            Lower,
            GreaterEqual,
            LowerEqual
        } ;

        RelationMember lhs;
        Operator op;
        RelationMember rhs;

        friend bool operator==(const Relation& eq_lhs, const Relation& eq_rhs)
        {
            return eq_lhs.lhs == eq_rhs.lhs && eq_lhs.rhs == eq_rhs.rhs && eq_lhs.op == eq_rhs.op;
        }

        QString toString() const;
};
}
