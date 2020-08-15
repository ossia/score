// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "Expression.hpp"

#include <State/AddressParser.hpp>
#include <State/ValueParser.hpp>
/*
Here is the grammar used. The grammar itself is split in multiple classes where
relevant.


# Addresses
device 			:= +[a-zA-Z0-9.~()_-];
fragment 		:= +[a-zA-Z0-9.~():_-];
path_element 	:= fragment;
path 			:= ('/', path_element)+ | '/';

Address 		:= device, ‘:’, path;
Dataspace   := 'color' || 'distance' || ...;
UnitQualifier: Dataspace, '.', Unit, ('.', UnitAccessor)?; // e.g. color.rgb or
color.rgb.r ; we make a static table with them precomputed.
AddressAccessor 	:= Address, '@', (('[', [:int:], ']')* || ('[',
UnitQualifier,
']'));



# Values
char			:= '\'', [:ascii:] - '\'', '\'';
str				:= '"', ([:ascii:] - '"')*, '"';
list			:= '[', (value % ','), ']';
bool			:= 'true' || 'false' ;
int				:= [:int:];
float			:= [:float:];
variant			:= char || str || list || bool || int || float;

Value 			:= variant;


# Relations
RelationMember	:= Value || Address;
RelationOp		:= '<=' || '<' || '>=' || '>' || '==' || '!=';

Relation		:= RelationMember, RelationOp, RelationMember;

Pulse           := 'impulse(' , Address , ')'

# Boolean operations

Expr 			:= Or;
Or				:= (Xor, 'or', Or) | Xor;
Xor				:= (And, 'xor', Xor) | And;
And				:= (Not, 'and', And) | Not;
Not				:= ('not', Simple) | Simple;

Simple			:= ('{', Expr, '}') | Relation;
*/

BOOST_FUSION_ADAPT_STRUCT(
    State::Relation,
    (State::RelationMember, lhs)(ossia::expressions::comparator, op)(State::RelationMember, rhs))

namespace
{
/// Address parsing.
namespace qi = boost::spirit::qi;

using boost::spirit::qi::rule;

//// RelMember parsing
template <typename Iterator>
struct RelationMember_parser : qi::grammar<Iterator, State::RelationMember()>
{
  RelationMember_parser() : RelationMember_parser::base_type(start)
  {
    start %= ("%" >> addracc >> "%") | ("%" >> addr >> "%") | val;
  }

  Address_parser<Iterator> addr;
  AddressAccessor_parser<Iterator> addracc;
  Value_parser<Iterator> val;
  qi::rule<Iterator, State::RelationMember()> start;
};

/// Relation parsing
struct RelationOperation_map : qi::symbols<char, ossia::expressions::comparator>
{
  RelationOperation_map()
  {
    add("<=", ossia::expressions::comparator::LOWER_EQUAL)(
        ">=", ossia::expressions::comparator::GREATER_EQUAL)(
        "<", ossia::expressions::comparator::LOWER)(">", ossia::expressions::comparator::GREATER)(
        "!=",
        ossia::expressions::comparator::DIFFERENT)("==", ossia::expressions::comparator::EQUAL);
  }
};

template <typename Iterator>
struct Relation_parser : qi::grammar<Iterator, State::Relation()>
{
  Relation_parser() : Relation_parser::base_type(start)
  {
    using boost::spirit::qi::skip;
    start %= skip(boost::spirit::standard::space)[(rm_parser >> op_map >> rm2_parser)];
  }

  RelationMember_parser<Iterator> rm_parser;
  RelationMember_parser<Iterator> rm2_parser;
  RelationOperation_map op_map;
  qi::rule<Iterator, State::Relation()> start;
};

template <typename Iterator>
struct Pulse_parser : qi::grammar<Iterator, State::Pulse()>
{
  Pulse_parser() : Pulse_parser::base_type(start)
  {
    using boost::spirit::qi::lit;
    using boost::spirit::qi::skip;
    using boost::spirit::standard::string;
    start %= skip(boost::spirit::standard::space)["%" >> addr >> "%" >> "impulse"]
             | skip(boost::spirit::standard::space)
                 [lit('{') >> lit("%") >> addr >> lit("%") >> lit("impulse") >> '}'];
  }

  Address_parser<Iterator> addr;
  qi::rule<Iterator, State::Pulse()> start;
};

// 90% of the boolean expr. parsing was taken from the stackoverflow answer :
// http://stackoverflow.com/a/8707598/1495627
namespace qi = boost::spirit::qi;
namespace phx = boost::phoenix;

struct op_or
{
};
struct op_and
{
};
struct op_xor
{
};
struct op_not
{
};

typedef std::string var;
template <typename tag>
struct binop;
template <typename tag>
struct unop;

using expr_raw = boost::variant<
    State::Relation,
    State::Pulse,
    boost::recursive_wrapper<unop<op_not>>,
    boost::recursive_wrapper<binop<op_and>>,
    boost::recursive_wrapper<binop<op_xor>>,
    boost::recursive_wrapper<binop<op_or>>>;

template <typename tag>
struct binop
{
  explicit binop(expr_raw l, expr_raw r) : oper1(std::move(l)), oper2(std::move(r)) { }
  expr_raw oper1, oper2;
};

template <typename tag>
struct unop
{
  explicit unop(expr_raw o) : oper1(std::move(o)) { }
  expr_raw oper1;
};

template <typename It, typename Skipper = qi::space_type>
struct Expression_parser : qi::grammar<It, expr_raw(), Skipper>
{
  Expression_parser() : Expression_parser::base_type(expr_)
  {
    using namespace qi;

    expr_ = or_.alias();

    namespace bsi = boost::spirit;
    or_ = (xor_ >> "or" >> or_)[_val = phx::construct<binop<op_or>>(bsi::_1, bsi::_2)]
          | xor_[_val = bsi::_1];
    xor_ = (and_ >> "xor" >> xor_)[_val = phx::construct<binop<op_xor>>(bsi::_1, bsi::_2)]
           | and_[_val = bsi::_1];
    and_ = (not_ >> "and" >> and_)[_val = phx::construct<binop<op_and>>(bsi::_1, bsi::_2)]
           | not_[_val = bsi::_1];
    not_ = ("not" > simple)[_val = phx::construct<unop<op_not>>(bsi::_1)] | simple[_val = bsi::_1];

    simple = (('{' >> expr_ >> '}') | relation_ | pulse_);
  }

private:
  Relation_parser<It> relation_;
  Pulse_parser<It> pulse_;
  qi::rule<It, expr_raw(), Skipper> not_, and_, xor_, or_, simple, expr_;
};

struct Expression_builder : boost::static_visitor<void>
{
  Expression_builder(State::Expression* e) : m_current{e} { }
  State::Expression* m_current{};

  void operator()(const State::Relation& rel) { m_current->emplace_back(rel, nullptr); }

  void operator()(const State::Pulse& rel) { m_current->emplace_back(rel, nullptr); }

  void operator()(const binop<op_and>& b)
  {
    rec_binop(State::BinaryOperator::AND, b.oper1, b.oper2);
  }
  void operator()(const binop<op_or>& b)
  {
    rec_binop(State::BinaryOperator::OR, b.oper1, b.oper2);
  }
  void operator()(const binop<op_xor>& b)
  {
    rec_binop(State::BinaryOperator::XOR, b.oper1, b.oper2);
  }

  void rec_binop(State::BinaryOperator binop, const expr_raw& l, const expr_raw& r)
  {
    m_current->emplace_back(binop, nullptr);

    auto old_expr = m_current;
    m_current = &old_expr->children().back();

    boost::apply_visitor(*this, l);
    boost::apply_visitor(*this, r);

    m_current = old_expr;
  }

  void operator()(const unop<op_not>& u)
  {
    m_current->emplace_back(State::UnaryOperator::Not, nullptr);

    auto old_expr = m_current;
    m_current = &old_expr->children().back();

    boost::apply_visitor(*this, u.oper1);

    m_current = old_expr;
  }
};
}

std::optional<State::Expression> State::parseExpression(const std::string& input)
{
  auto f(std::begin(input)), l(std::end(input));
  auto p = std::make_unique<Expression_parser<decltype(f)>>();
  try
  {
    expr_raw result;
    bool ok = qi::phrase_parse(f, l, *p, qi::standard::space, result);

    if (!ok)
    {
      return {};
    }

    State::Expression e;

    Expression_builder bldr{&e};
    boost::apply_visitor(bldr, result);

    return e;
  }
  catch (const qi::expectation_failure<decltype(f)>& e)
  {
    // SCORE_BREAKPOINT;
    return {};
  }
  catch (...)
  {
    // SCORE_BREAKPOINT;
    return {};
  }
}

std::optional<State::Expression> State::parseExpression(const QString& str)
{
  return parseExpression(str.toStdString());
}
