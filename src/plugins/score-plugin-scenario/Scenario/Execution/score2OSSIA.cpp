// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionFunctions.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/detail/apply.hpp>
#include <ossia/editor/expression/expression.hpp>
#include <ossia/editor/expression/expression_atom.hpp>
#include <ossia/editor/expression/expression_composition.hpp>
#include <ossia/editor/expression/expression_not.hpp>
#include <ossia/editor/expression/expression_pulse.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>
#include <ossia/network/value/value.hpp>

class NodeNotFoundException : public std::runtime_error
{
public:
  NodeNotFoundException(const State::Address& n)
      : std::runtime_error{
          "Address: '" + n.toString().toStdString() + "' not found in actual tree."}
  {
  }
};

namespace Engine
{
namespace score_to_ossia
{

ossia::net::parameter_base*
address(const State::Address& addr, const ossia::execution_state& deviceList)
{
  // OPTIMIZEME by sorting by device prior
  // to this.
  auto n = Execution::findNode(deviceList, addr);
  if (n)
    return n->get_parameter();
  return nullptr;
}

optional<ossia::message>
message(const State::Message& mess, const ossia::execution_state& deviceList)
{
  if (auto ossia_addr = address(mess.address.address, deviceList))
  {
    if (mess.value.valid())
      return ossia::message{
          {*ossia_addr, mess.address.qualifiers.get().accessors},
          mess.value,
          mess.address.qualifiers.get().unit};
  }

  return {};
}

void state(
    ossia::state& parent,
    const Scenario::StateModel& score_state,
    const Execution::Context& ctx)
{
  auto& elts = parent;

  // For all elements where IOType != Invalid,
  // we add the elements to the state.

  auto& dl = *ctx.execState;
  score_state.messages().rootNode().visit([&](const auto& n) {
    const auto& val = n.value();
    if (val)
    {
      elts.add(message(State::Message{Process::address(n), *val}, dl));
    }
  });

  /* TODO
  for (auto& proc : score_state.stateProcesses)
  {
    auto fac = ctx.stateProcesses.factory(proc);
    if (fac)
    {
      elts.add(fac->make(proc, ctx));
    }
  }
  */
}

ossia::state state(const Scenario::StateModel& score_state, const Execution::Context& ctx)
{
  ossia::state s;
  Engine::score_to_ossia::state(s, score_state, ctx);
  return s;
}

static ossia::destination
expressionAddress(const State::Address& addr, const ossia::execution_state& devlist)
{
  auto n = Execution::findNode(devlist, addr);
  if (n)
  {
    auto ossia_addr = n->get_parameter();
    if (ossia_addr)
      return ossia::destination(*ossia_addr);
    else
      throw NodeNotFoundException(addr);
  }
  else
  {
    throw NodeNotFoundException(addr);
  }
}

static ossia::expressions::expression_atom::val_t
expressionOperand(const State::RelationMember& relm, const ossia::execution_state& list)
{
  using namespace eggs::variants;

  const struct
  {
  public:
    const ossia::execution_state& devlist;
    using return_type = ossia::expressions::expression_atom::val_t;
    return_type operator()(const State::Address& addr) const
    {
      return expressionAddress(addr, devlist);
    }

    return_type operator()(const ossia::value& val) const { return val; }

    return_type operator()(const State::AddressAccessor& acc) const
    {
      auto dest = expressionAddress(acc.address, devlist);
      dest.index = acc.qualifiers.get().accessors;
      dest.unit = acc.qualifiers.get().unit;
      return dest;
    }
  } visitor{list};

  return eggs::variants::apply(visitor, relm);
}

// State::Relation -> OSSIA::ExpressionAtom
static ossia::expression_ptr
expressionAtom(const State::Relation& rel, const ossia::execution_state& dev)
{
  using namespace eggs::variants;

  return ossia::expressions::make_expression_atom(
      expressionOperand(rel.lhs, dev), rel.op, expressionOperand(rel.rhs, dev));
}

static ossia::expression_ptr
expressionPulse(const State::Pulse& rel, const ossia::execution_state& dev)
{
  using namespace eggs::variants;

  return ossia::expressions::make_expression_pulse(expressionAddress(rel.address, dev));
}

template <typename T>
ossia::expression_ptr
expression(const State::Expression& e, const ossia::execution_state& list, const T&)
{
  const struct
  {
    const State::Expression& expr;
    const ossia::execution_state& devlist;

    ossia::expression_ptr operator()() const { return T::default_expression(); }

    ossia::expression_ptr operator()(const State::Relation& rel) const
    {
      return expressionAtom(rel, devlist);
    }
    ossia::expression_ptr operator()(const State::Pulse& rel) const
    {
      return expressionPulse(rel, devlist);
    }

    ossia::expression_ptr operator()(const State::BinaryOperator rel) const
    {
      const auto& lhs = expr.childAt(0);
      const auto& rhs = expr.childAt(1);
      return ossia::expressions::make_expression_composition(
          condition_expression(lhs, devlist), rel, condition_expression(rhs, devlist));
    }
    ossia::expression_ptr operator()(const State::UnaryOperator) const
    {
      return ossia::expressions::make_expression_not(
          condition_expression(expr.childAt(0), devlist));
    }
    ossia::expression_ptr operator()(const InvisibleRootNode) const
    {
      if (expr.childCount() == 0)
      {
        // By default no expression == true
        return T::default_expression();
      }
      else if (expr.childCount() == 1)
      {
        return condition_expression(expr.childAt(0), devlist);
      }
      else
      {
        SCORE_ABORT;
      }
    }

  } visitor{e, list};

  return ossia::apply(visitor, e.impl());
}

ossia::expression_ptr
condition_expression(const State::Expression& e, const ossia::execution_state& list)
{
  struct def_cond
  {
    static ossia::expression_ptr default_expression()
    {
      return ossia::expressions::make_expression_true();
    }
  };
  return expression(e, list, def_cond{});
}
ossia::expression_ptr
trigger_expression(const State::Expression& e, const ossia::execution_state& list)
{
  struct def_trig
  {
    static ossia::expression_ptr default_expression()
    {
      return ossia::expressions::make_expression_false();
    }
  };

  return expression(e, list, def_trig{});
}
}
}
