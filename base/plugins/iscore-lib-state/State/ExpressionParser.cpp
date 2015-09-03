#include "Expression.hpp"
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_real.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/variant/recursive_wrapper.hpp>
#include <boost/fusion/adapted.hpp>

/*
Here is the grammar used. The grammar itself is split in multiple classes where
relevant.


# Addresses
alnum 			:= +[a-zA-Z0-9];
device 			:= alnum;
path_element 	:= alnum;
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
        iscore::Address,
        (QString, device)
        (QStringList, path)
        )

BOOST_FUSION_ADAPT_STRUCT(
        iscore::Value,
        (QVariant, val)
        )

BOOST_FUSION_ADAPT_STRUCT(
        iscore::Relation,
        (iscore::RelationMember, lhs)
        (iscore::Relation::Operator, op)
        (iscore::RelationMember, rhs)
        )
namespace {
/// Address parsing.
using namespace boost::fusion;
namespace qi = boost::spirit::qi;

using namespace boost::phoenix;
using boost::spirit::qi::rule;

template <typename Iterator>
struct Address_parser : qi::grammar<Iterator, iscore::Address()>
{
    Address_parser() : Address_parser::base_type(start)
    {
        using qi::alnum;

        dev = +alnum;
        member_elt = +alnum;
        path %= (
                    +("/" >> member_elt)
                    | "/"
                    );
        start %= dev >> ":" >> path;

    }

    qi::rule<Iterator, QString()> dev;
    qi::rule<Iterator, QString()> member_elt;
    qi::rule<Iterator, QStringList()> path;
    qi::rule<Iterator, iscore::Address()> start;
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
struct Value_parser : qi::grammar<Iterator, iscore::Value()>
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

        bool_parser %=
        // TODO true/false (cf. bool_)
        tuple_parser %= skip(boost::spirit::ascii::space) [ "[" >> (variant % ",") >> "]" ];
        variant %=  real_parser<float, boost::spirit::qi::strict_real_policies<float> >()
                | int_
                | bool_parser
                | char_parser
                | str_parser
                | tuple_parser;

        start %= variant;
    }

    BoolParse_map bool_parser;

    qi::rule<Iterator, QVariantList()> tuple_parser;
    qi::rule<Iterator, QChar()> char_parser;
    qi::rule<Iterator, QString()> str_parser;
    qi::rule<Iterator, QVariant()> variant;
    qi::rule<Iterator, iscore::Value()> start;
};


//// RelMember parsing
template <typename Iterator>
struct RelationMember_parser : qi::grammar<Iterator, iscore::RelationMember()>
{
    RelationMember_parser() : RelationMember_parser::base_type(start)
    {
        start %= addr | val;
    }

    Address_parser<Iterator> addr;
    Value_parser<Iterator> val;
    qi::rule<Iterator, iscore::RelationMember()> start;
};

/// Relation parsing
struct RelationOperation_map : qi::symbols<char, iscore::Relation::Operator>
{
        RelationOperation_map() {
            add
                    ("<=", iscore::Relation::Operator::LowerEqual)
                    (">=", iscore::Relation::Operator::GreaterEqual)
                    ("<" , iscore::Relation::Operator::Lower)
                    (">" , iscore::Relation::Operator::Greater)
                    ("!=", iscore::Relation::Operator::Different)
                    ("==", iscore::Relation::Operator::Equal)
                    ;
        }
};

template <typename Iterator>
struct Relation_parser : qi::grammar<Iterator, iscore::Relation()>
{
    Relation_parser() : Relation_parser::base_type(start)
    {
        using boost::spirit::qi::skip;
        start %= skip(boost::spirit::ascii::space) [ rm_parser
                >> op_map
                >> rm_parser ];

    }

    RelationMember_parser<Iterator> rm_parser;
    RelationOperation_map op_map;
    qi::rule<Iterator, iscore::Relation()> start;
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

using expr_raw = boost::variant<iscore::Relation,
        boost::recursive_wrapper<unop <op_not> >,
        boost::recursive_wrapper<binop<op_and> >,
        boost::recursive_wrapper<binop<op_xor> >,
        boost::recursive_wrapper<binop<op_or> >
        >;

template <typename tag> struct binop
{
    explicit binop(const expr_raw& l, const expr_raw& r) : oper1(l), oper2(r) { }
    expr_raw oper1, oper2;
};

template <typename tag> struct unop
{
    explicit unop(const expr_raw& o) : oper1(o) { }
    expr_raw oper1;
};

template <typename It, typename Skipper = qi::space_type>
    struct Expression_parser : qi::grammar<It, expr_raw(), Skipper>
{
    Expression_parser() : Expression_parser::base_type(expr_)
    {
        using namespace qi;

        expr_  = or_.alias();

        or_  = (xor_ >> "or"  >> or_ ) [ _val = phx::construct<binop<op_or >>(_1, _2) ] | xor_   [ _val = _1 ];
        xor_ = (and_ >> "xor" >> xor_) [ _val = phx::construct<binop<op_xor>>(_1, _2) ] | and_   [ _val = _1 ];
        and_ = (not_ >> "and" >> and_) [ _val = phx::construct<binop<op_and>>(_1, _2) ] | not_   [ _val = _1 ];
        not_ = ("not" > simple       ) [ _val = phx::construct<unop <op_not>>(_1)     ] | simple [ _val = _1 ];

        simple = (('(' > expr_ > ')') | var_);
    }

  private:
    Relation_parser<It> var_;
    qi::rule<It, expr_raw(), Skipper> not_, and_, xor_, or_, simple, expr_;
};

struct Expression_builder : boost::static_visitor<void>
{
            Expression_builder(iscore::Expression* e):m_current{e}{}
        iscore::Expression* m_current{};

        void operator()(const iscore::Relation& rel)
        {
            m_current->addChild(new iscore::Expression(rel));
        }

        void operator()(const binop<op_and>& b)
        {
            auto new_expr = new iscore::Expression(iscore::BinaryOperator::And);
            print(new_expr, b.oper1, b.oper2);
        }
        void operator()(const binop<op_or >& b)
        {
            auto new_expr = new iscore::Expression(iscore::BinaryOperator::Or);
            print(new_expr, b.oper1, b.oper2);
        }
        void operator()(const binop<op_xor>& b)
        {
            auto new_expr = new iscore::Expression(iscore::BinaryOperator::Xor);
            print(new_expr, b.oper1, b.oper2);
        }

        void print(iscore::Expression* new_expr, const expr_raw& l, const expr_raw& r)
        {
            m_current->addChild(new_expr);

            auto old_expr = m_current;
            m_current = new_expr;

            boost::apply_visitor(*this, l);
            boost::apply_visitor(*this, r);

            m_current = old_expr;
        }

        void operator()(const unop<op_not>& u)
        {
            auto new_expr = new iscore::Expression(iscore::UnaryOperator::Not);
            m_current->addChild(new_expr);

            auto old_expr = m_current;
            m_current = new_expr;

            boost::apply_visitor(*this, u.oper1);

            m_current = old_expr;
        }
};
}


    boost::optional<iscore::Expression> iscore::parse(const QString& str)
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

            iscore::Expression e;

            Expression_builder bldr{&e};
            boost::apply_visitor(bldr, result);

            return e;

        }
        catch (const qi::expectation_failure<decltype(f)>& e)
        {
            return {};
        }
        catch(...)
        {
            return {};
        }

        return {};
    }
