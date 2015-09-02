#include "Expression.hpp"
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_real.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/variant/recursive_wrapper.hpp>
#include <boost/fusion/adapted.hpp>

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

/// Address parsing.
using namespace boost::fusion;
namespace qi = boost::spirit::qi;

using namespace boost::phoenix;
using boost::spirit::qi::rule;

BOOST_FUSION_ADAPT_STRUCT(
        iscore::Address,
        (QString, device)
        (QStringList, path)
        )
template <typename Iterator>
struct address_parser : qi::grammar<Iterator, iscore::Address()>
{
    address_parser() : address_parser::base_type(start)
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
BOOST_FUSION_ADAPT_STRUCT(
        iscore::Value,
        (QVariant, val)
        )
template <typename Iterator>
struct value_parser : qi::grammar<Iterator, iscore::Value()>
{
    value_parser() : value_parser::base_type(start)
    {
        using qi::alnum;
        using boost::spirit::qi::skip;
        using boost::spirit::int_;
        using boost::spirit::qi::real_parser;
        using boost::spirit::qi::char_;

        char_parser %= "'" >> (char_ - "'") >> "'";
        str_parser %= '"' >> qi::lexeme [ +(char_ - '"') ] >> '"';

        // TODO true/false (cf. bool_)
        tuple_parser %= skip(boost::spirit::ascii::space) [ "[" >> (variant % ",") >> "]" ];
        variant %=  real_parser<float, boost::spirit::qi::strict_real_policies<float> >()
                | int_
                | char_parser
                | str_parser
                | tuple_parser;

        start %= variant;
    }

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

    address_parser<Iterator> addr;
    value_parser<Iterator> val;
    qi::rule<Iterator, iscore::RelationMember()> start;
};



/// Relation parsing
BOOST_FUSION_ADAPT_STRUCT(
        iscore::Relation,
        (iscore::RelationMember, lhs)
        (iscore::Relation::Operator, op)
        (iscore::RelationMember, rhs)
        )
struct operations_map : qi::symbols<char, iscore::Relation::Operator>
{
        operations_map() {
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
    operations_map op_map;
    qi::rule<Iterator, iscore::Relation()> start;
};



void expr_parse_test()
{

}
