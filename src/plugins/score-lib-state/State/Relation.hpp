#pragma once

#include <State/Address.hpp>
#include <State/Value.hpp>

#include <ossia/editor/expression/operators.hpp>

#include <QString>

#include <eggs/variant.hpp>

namespace State
{
using RelationMember = eggs::variant<State::Address, State::AddressAccessor, ossia::value>;

SCORE_LIB_STATE_EXPORT QString toString(const RelationMember&);

struct SCORE_LIB_STATE_EXPORT Relation
{
  Relation() noexcept = default;
  Relation(const Relation& other) noexcept : lhs{other.lhs}, op{other.op}, rhs{other.rhs} { }

  Relation(Relation&& other) noexcept
      : lhs{std::move(other.lhs)}, op{other.op}, rhs{std::move(other.rhs)}
  {
  }

  Relation& operator=(const Relation& other) noexcept
  {
    lhs = other.lhs;
    op = other.op;
    rhs = other.rhs;
    return *this;
  }
  Relation& operator=(Relation&& other) noexcept
  {
    lhs = std::move(other.lhs);
    op = other.op;
    rhs = std::move(other.rhs);
    return *this;
  }

  Relation(RelationMember l, ossia::expressions::comparator o, RelationMember r)
      : lhs{std::move(l)}, op{o}, rhs{std::move(r)}
  {
  }

  RelationMember lhs;
  ossia::expressions::comparator op;
  RelationMember rhs;

  friend bool operator==(const Relation& eq_lhs, const Relation& eq_rhs)
  {
    return eq_lhs.lhs == eq_rhs.lhs && eq_lhs.rhs == eq_rhs.rhs && eq_lhs.op == eq_rhs.op;
  }
};

SCORE_LIB_STATE_EXPORT QString toString(const Relation&);

struct SCORE_LIB_STATE_EXPORT Pulse
{
  State::Address address;

  friend bool operator==(const Pulse& lhs, const Pulse& rhs) { return lhs.address == rhs.address; }
};
SCORE_LIB_STATE_EXPORT QString toString(const Pulse&);
SCORE_LIB_STATE_EXPORT const QMap<ossia::expressions::comparator, QString> opToString();
}
