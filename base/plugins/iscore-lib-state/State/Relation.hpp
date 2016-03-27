#pragma once

#include <eggs/variant/variant.hpp>
#include <QString>
#include <State/Address.hpp>
#include <State/Value.hpp>

namespace State
{
using RelationMember = eggs::variant<
    State::Address,
    State::AddressAccessor,
    State::Value>;

ISCORE_LIB_STATE_EXPORT QString toString(const RelationMember&);

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
};

ISCORE_LIB_STATE_EXPORT QString toString(const Relation&);

struct ISCORE_LIB_STATE_EXPORT Pulse
{
        State::Address address;

        friend bool operator==(const Pulse& lhs, const Pulse& rhs)
        {
            return lhs.address == rhs.address;
        }

};
ISCORE_LIB_STATE_EXPORT QString toString(const Pulse&);
ISCORE_LIB_STATE_EXPORT const QMap<State::Relation::Operator, QString> opToString();

}
