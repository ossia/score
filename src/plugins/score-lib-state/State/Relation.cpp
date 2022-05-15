// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Relation.hpp"

#include <State/ValueConversion.hpp>

#include <ossia/network/common/destination_qualifiers.hpp>

#include <score/tools/Debug.hpp>
#include <QMap>
namespace State
{

QString toString(const Pulse& pulse)
{
  return "%" + pulse.address.toString() + "% impulse";
}

} // namespace score

QString State::toString(const State::RelationMember& m)
{
  struct
  {
  public:
    using return_type = QString;
    return_type operator()(const State::Address& addr) const
    {
      return "%" + addr.toString() + "%";
    }

    return_type operator()(const ossia::value& val) const
    {
      return State::convert::toPrettyString(val);
    }

    return_type operator()(const State::AddressAccessor& acc) const
    {
      auto addr = acc.address.toString();

      if (!acc.qualifiers.get().accessors.empty())
      {
        addr += '@';
        for (auto val : acc.qualifiers.get().accessors)
        {
          addr += QString("[%1]").arg(val);
         }
      }

      return "%" + addr + "%";
    }

    return_type operator()(ossia::monostate) const
    {
      SCORE_ASSERT(false);
      return {};
    }
  } visitor{};

  return ossia::visit(visitor, m);
}

QString State::toString(const Relation& rel)
{
  return QString("%1 %2 %3")
      .arg(toString(rel.lhs))
      .arg(opToString()[rel.op])
      .arg(toString(rel.rhs));
}

const QMap<ossia::expressions::comparator, QString> State::opToString()
{
  return {
      {ossia::expressions::comparator::LOWER_EQUAL, "<="},
      {ossia::expressions::comparator::GREATER_EQUAL, ">="},
      {ossia::expressions::comparator::LOWER, "<"},
      {ossia::expressions::comparator::GREATER, ">"},
      {ossia::expressions::comparator::DIFFERENT, "!="},
      {ossia::expressions::comparator::EQUAL, "=="}};
}
