#include "Expression.hpp"
#ifndef Q_MOC_RUN
#define BOOST_SPIRIT_DEBUG
// see https://svn.boost.org/trac/boost/ticket/11875
#if defined(_GLIBCXX_DEBUG)
#define BOOST_PHOENIX_USING_LIBCPP
#endif
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_real.hpp>
#include <boost/spirit/include/qi_lit.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/variant/recursive_wrapper.hpp>
#include <boost/fusion/adapted.hpp>
#include <iscore/prefix.hpp>
#endif
/*
Here is the grammar used. The grammar itself is split in multiple classes where
relevant.


# Addresses
fragment 		:= +[a-zA-Z0-9.~()_-];
device 			:= fragment;
path_element 	:= fragment;
path 			:= ('/', path_element)+ | '/';

Address 		:= device, ‘:’, path;


# Values
char			:= '\'', [:ascii:] - '\'', '\'';
str				:= '"', ([:ascii:] - '"')*, '"';
tuple			:= '[', (value % ','), ']';
bool			:= 'true' || 'false' ;
int				:= [:int:];
float			:= [:float:];
variant			:= char || str || tuple || bool || int || float;

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

Simple			:= ('(', Expr, ')') | Relation;
*/

// Taken from boost doc, necessary to have support of QString
namespace boost { namespace spirit { namespace traits
{
// Make Qi recognize QString as a container
template <> struct is_container<QString> : mpl::true_ {};

// Expose the container's (QString's) value_type
template <> struct container_value<QString> : mpl::identity<QChar> {};

// Define how to insert a new element at the end of the container (QString)
template <>
struct push_back_container<QString, QChar>
{
        static bool call(QString& c, QChar const& val)
        {
            c.append(val);
            return true;
        }
};

// Test if a QString is empty (required for debug)
template <>
struct is_empty_container<QString>
{
        static bool call(QString const& c)
        {
            return c.isEmpty();
        }
};

// Define how to stream a QString (required for debug)
template <typename Out, typename Enable>
struct print_attribute_debug<Out, QString, Enable>
{
        static void call(Out& out, QString const& val)
        {
            out << val.toStdString();
        }
};
}}}


BOOST_FUSION_ADAPT_STRUCT(
        State::Address,
        (QString, device)
        (QStringList, path)
        )

BOOST_FUSION_ADAPT_STRUCT(
        State::AddressAccessor,
        (State::Address, address)
        (std::vector<int32_t>, accessors)
        )

BOOST_FUSION_ADAPT_STRUCT(
        State::Value,
        (State::Value::value_type, val)
        )

BOOST_FUSION_ADAPT_STRUCT(
        State::Relation,
        (State::RelationMember, lhs)
        (State::Relation::Operator, op)
        (State::RelationMember, rhs)
        )

BOOST_FUSION_ADAPT_STRUCT(
        State::Pulse,
        (State::Address, address)
        )
namespace {
/// Address parsing.
using namespace boost::fusion;
namespace qi = boost::spirit::qi;

using namespace boost::phoenix;
using boost::spirit::qi::rule;

template <typename Iterator>
struct Address_parser : qi::grammar<Iterator, State::Address()>
{
    Address_parser() : Address_parser::base_type(start)
    {
        using qi::alnum;
        dev = +qi::char_("a-zA-Z0-9_~().-");
        member_elt = +qi::char_("a-zA-Z0-9_~().-");
        path %= (
                    +("/" >> member_elt)
                    | "/"
                    );
        start %= dev >> ":" >> path;

    }

    qi::rule<Iterator, QString()> dev;
    qi::rule<Iterator, QString()> member_elt;
    qi::rule<Iterator, QStringList()> path;
    qi::rule<Iterator, State::Address()> start;
};

template <typename Iterator>
struct AccessorList_parser : qi::grammar<Iterator, std::vector<int32_t>()>
{
    AccessorList_parser() : AccessorList_parser::base_type(start)
    {
        using qi::alnum;
        using boost::spirit::qi::skip;
        using boost::spirit::ascii::space;
        using boost::spirit::int_;

        index %= skip(space) [ "[" >> int_ >> "]" ];
        start %= skip(space) [ +(index) ];
    }

    qi::rule<Iterator, std::vector<int32_t>()> start;
    qi::rule<Iterator, int32_t()> index;
};


template <typename Iterator>
struct AddressAccessor_parser : qi::grammar<Iterator, State::AddressAccessor()>
{
    AddressAccessor_parser() : AddressAccessor_parser::base_type(start)
    {
        using qi::alnum;
        using boost::spirit::qi::skip;
        using boost::spirit::ascii::space;
        using boost::spirit::int_;

        start %= skip(space) [ address >> accessors ];
    }

    qi::rule<Iterator, State::AddressAccessor()> start;
    Address_parser<Iterator> address;
    AccessorList_parser<Iterator> accessors;
};


/// Value parsing
struct BoolParse_map : qi::symbols<char, bool>
{
        BoolParse_map() {
            add
                    ("true", true)
                    ("false", false)
                    ;
        }
};
template <typename Iterator>
struct Value_parser : qi::grammar<Iterator, State::Value()>
{
    Value_parser() : Value_parser::base_type(start)
    {
        using qi::alnum;
        using boost::spirit::qi::skip;
        using boost::spirit::int_;
        using boost::spirit::qi::real_parser;
        using boost::spirit::qi::char_;

        char_parser %= "'" >> (char_ - "'") >> "'";
        str_parser %= '"' >> qi::lexeme [ *(char_ - '"') ] >> '"';

        tuple_parser %= skip(boost::spirit::ascii::space) [ "[" >> variant % "," >> "]" ];
        variant %=  real_parser<float, boost::spirit::qi::strict_real_policies<float> >()
                | int_
                | bool_parser
                | char_parser
                | str_parser
                | tuple_parser;

        start %= variant;
    }

    BoolParse_map bool_parser;

    qi::rule<Iterator, State::tuple_t()> tuple_parser;
    qi::rule<Iterator, QChar()> char_parser;
    qi::rule<Iterator, QString()> str_parser;
    qi::rule<Iterator, State::Value::value_type()> variant;
    qi::rule<Iterator, State::Value()> start;
};


//// RelMember parsing
template <typename Iterator>
struct RelationMember_parser : qi::grammar<Iterator, State::RelationMember()>
{
    RelationMember_parser() : RelationMember_parser::base_type(start)
    {
        start %= addracc | addr | val;
    }

    Address_parser<Iterator> addr;
    AddressAccessor_parser<Iterator> addracc;
    Value_parser<Iterator> val;
    qi::rule<Iterator, State::RelationMember()> start;
};

/// Relation parsing
struct RelationOperation_map : qi::symbols<char, State::Relation::Operator>
{
        RelationOperation_map() {
            add
                    ("<=", State::Relation::Operator::LowerEqual)
                    (">=", State::Relation::Operator::GreaterEqual)
                    ("<" , State::Relation::Operator::Lower)
                    (">" , State::Relation::Operator::Greater)
                    ("!=", State::Relation::Operator::Different)
                    ("==", State::Relation::Operator::Equal)
                    ;
        }
};


template <typename Iterator>
struct Relation_parser : qi::grammar<Iterator, State::Relation()>
{
    Relation_parser() : Relation_parser::base_type(start)
    {
        using boost::spirit::qi::skip;
        start %= skip(boost::spirit::ascii::space) [
                 (rm_parser >> op_map >> rm_parser)
        ];

    }

    RelationMember_parser<Iterator> rm_parser;
    RelationOperation_map op_map;
    qi::rule<Iterator, State::Relation()> start;
};

template <typename Iterator>
struct Pulse_parser : qi::grammar<Iterator, State::Pulse()>
{
    Pulse_parser() : Pulse_parser::base_type(start)
    {
        using boost::spirit::qi::skip;
        using boost::spirit::qi::lit;
        using boost::spirit::ascii::string;
        start %= skip(boost::spirit::ascii::space) [
                addr >> "impulse"
        ];
    }

    Address_parser<Iterator> addr;
    qi::rule<Iterator, State::Pulse()> start;
};





// 90% of the boolean expr. parsing was taken from the stackoverflow answer :
// http://stackoverflow.com/a/8707598/1495627
namespace qi    = boost::spirit::qi;
namespace phx   = boost::phoenix;

struct op_or  {};
struct op_and {};
struct op_xor {};
struct op_not {};

typedef std::string var;
template <typename tag> struct binop;
template <typename tag> struct unop;

using expr_raw = boost::variant<
        State::Relation,
        State::Pulse,
        boost::recursive_wrapper<unop <op_not> >,
        boost::recursive_wrapper<binop<op_and> >,
        boost::recursive_wrapper<binop<op_xor> >,
        boost::recursive_wrapper<binop<op_or> >
        >;

template <typename tag> struct binop
{
    explicit binop(expr_raw l, expr_raw r) :
            oper1(std::move(l)),
            oper2(std::move(r))
        { }
    expr_raw oper1, oper2;
};

template <typename tag> struct unop
{
    explicit unop(expr_raw o) :
            oper1(std::move(o))
        { }
    expr_raw oper1;
};

template <typename It, typename Skipper = qi::space_type>
    struct Expression_parser : qi::grammar<It, expr_raw(), Skipper>
{
    Expression_parser() : Expression_parser::base_type(expr_)
    {
        using namespace qi;

        expr_  = or_.alias();

        namespace bsi = boost::spirit;
        or_  = (xor_ >> "or"  >> or_ ) [ _val = phx::construct<binop<op_or >>(bsi::_1, bsi::_2) ] | xor_   [ _val = bsi::_1 ];
        xor_ = (and_ >> "xor" >> xor_) [ _val = phx::construct<binop<op_xor>>(bsi::_1, bsi::_2) ] | and_   [ _val = bsi::_1 ];
        and_ = (not_ >> "and" >> and_) [ _val = phx::construct<binop<op_and>>(bsi::_1, bsi::_2) ] | not_   [ _val = bsi::_1 ];
        not_ = ("not" > simple       ) [ _val = phx::construct<unop <op_not>>(bsi::_1)     ] | simple [ _val = bsi::_1 ];

        simple = (('(' > (expr_) > ')') | relation_ | pulse_);
    }

  private:
    Relation_parser<It> relation_;
    Pulse_parser<It> pulse_;
    qi::rule<It, expr_raw(), Skipper> not_, and_, xor_, or_, simple, expr_;
};

struct Expression_builder : boost::static_visitor<void>
{
            Expression_builder(State::Expression* e):m_current{e}{}
        State::Expression* m_current{};

        void operator()(const State::Relation& rel)
        {
            m_current->emplace_back(rel, nullptr);
        }

        void operator()(const State::Pulse& rel)
        {
            m_current->emplace_back(rel, nullptr);
        }

        void operator()(const binop<op_and>& b)
        {
            rec_binop(State::BinaryOperator::And, b.oper1, b.oper2);
        }
        void operator()(const binop<op_or >& b)
        {
            rec_binop(State::BinaryOperator::Or, b.oper1, b.oper2);
        }
        void operator()(const binop<op_xor>& b)
        {
            rec_binop(State::BinaryOperator::Xor, b.oper1, b.oper2);
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


    iscore::optional<State::Expression> State::parseExpression(const QString& str)
    {
        auto input = str.toStdString();
        auto f(std::begin(input)), l(std::end(input));
        Expression_parser<decltype(f)> p;
        try
        {
            expr_raw result;
            bool ok = qi::phrase_parse(f, l , p, qi::space, result);

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
            //ISCORE_BREAKPOINT;
            return {};
        }
        catch(...)
        {
            //ISCORE_BREAKPOINT;
            return {};
        }

        return {};
    }


    iscore::optional<State::Value> State::parseValue(const QString& str)
    {
        auto input = str.toStdString();
        auto f(std::begin(input)), l(std::end(input));
        Value_parser<decltype(f)> p;
        try
        {
            State::Value result;
            bool ok = qi::phrase_parse(f, l , p, qi::space, result);

            if (!ok)
            {
                return {};
            }

            return result;

        }
        catch (const qi::expectation_failure<decltype(f)>& e)
        {
            //ISCORE_BREAKPOINT;
            return {};
        }
        catch(...)
        {
            //ISCORE_BREAKPOINT;
            return {};
        }

        return {};
    }



    iscore::optional<State::Address> State::parseAddress(const QString& str)
    {
        auto input = str.toStdString();
        auto f(std::begin(input)), l(std::end(input));
        Address_parser<decltype(f)> p;
        try
        {
            State::Address result;
            bool ok = qi::phrase_parse(f, l , p, qi::space, result);

            if (!ok)
            {
                return {};
            }

            return result;

        }
        catch (const qi::expectation_failure<decltype(f)>& e)
        {
            //ISCORE_BREAKPOINT;
            return {};
        }
        catch(...)
        {
            //ISCORE_BREAKPOINT;
            return {};
        }

        return {};
    }

    iscore::optional<State::AddressAccessor> State::parseAddressAccessor(const QString& str)
    {
        auto input = str.toStdString();
        auto f(std::begin(input)), l(std::end(input));
        AddressAccessor_parser<decltype(f)> p;
        try
        {
            State::AddressAccessor result;
            bool ok = qi::phrase_parse(f, l , p, qi::space, result);

            if (ok)
            {
                return result;
            }
            else
            {
                // We try to get an address instead.
                iscore::optional<State::Address> res = State::parseAddress(str);
                if(res)
                {
                    result.address = (*res);
                    result.accessors.clear();

                    return result;
                }
                else
                {
                    return {};
                }
            }


        }
        catch (const qi::expectation_failure<decltype(f)>& e)
        {
            //ISCORE_BREAKPOINT;
            return {};
        }
        catch(...)
        {
            //ISCORE_BREAKPOINT;
            return {};
        }

        return {};
    }
