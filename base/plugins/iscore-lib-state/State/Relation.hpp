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

        friend bool operator==(const Relation& lhs, const Relation& rhs)
        {
            return lhs.lhs == rhs.lhs && lhs.rhs == rhs.rhs && lhs.op == rhs.op;
        }

        QString toString() const;
};
}
