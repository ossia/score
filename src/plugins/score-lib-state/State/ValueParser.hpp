#pragma once
#ifndef Q_MOC_RUN
//#define BOOST_SPIRIT_DEBUG
// see https://svn.boost.org/trac/boost/ticket/11875
#if defined(_GLIBCXX_DEBUG)
#define BOOST_PHOENIX_USING_LIBCPP
#endif

#include <State/Value.hpp>

#include <ossia/network/base/name_validation.hpp>
#include <ossia/network/dataspace/dataspace_parse.hpp>

#include <boost/fusion/adapted.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_eoi.hpp>
#include <boost/spirit/include/qi_lit.hpp>
#include <boost/spirit/include/qi_real.hpp>
#include <boost/spirit/repository/include/qi_confix.hpp>
#include <boost/variant/recursive_wrapper.hpp>

#include <QString>
#endif

// Taken from boost doc, necessary to have support of QString
namespace boost
{
namespace spirit
{
namespace traits
{
// Make Qi recognize QString as a container
template <>
struct is_container<QString> : mpl::true_
{
};

// Expose the container's (QString's) value_type
template <>
struct container_value<QString> : mpl::identity<QChar>
{
};

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
  static bool call(QString const& c) { return c.isEmpty(); }
};

// Define how to stream a QString (required for debug)
template <typename Out, typename Enable>
struct print_attribute_debug<Out, QString, Enable>
{
  static void call(Out& out, QString const& val) { out << val.toStdString(); }
};
}
}
}

namespace
{
/// Address parsing.
namespace qi = boost::spirit::qi;

using boost::spirit::qi::rule;

/// Value parsing
struct BoolParse_map : qi::symbols<char, bool>
{
  BoolParse_map() { add("true", true)("false", false); }
};
template <typename Iterator>
struct Value_parser : qi::grammar<Iterator, ossia::value()>
{
  Value_parser() : Value_parser::base_type(start)
  {
    using boost::spirit::int_;
    using boost::spirit::qi::char_;
    using boost::spirit::qi::real_parser;
    using boost::spirit::qi::skip;
    using qi::alnum;

    char_parser %= "'" >> (char_ - "'") >> "'";
    str_parser %= '"' >> qi::lexeme[*(char_ - '"')] >> '"';

    list_parser %= skip(boost::spirit::standard::space)["[" >> start % "," >> "]"];
    start %= real_parser<float, boost::spirit::qi::strict_real_policies<float>>() | int_
             | bool_parser | char_parser | str_parser | list_parser;
  }

  BoolParse_map bool_parser;

  qi::rule<Iterator, std::vector<ossia::value>()> list_parser;
  qi::rule<Iterator, char()> char_parser;
  qi::rule<Iterator, std::string()> str_parser;
  qi::rule<Iterator, ossia::value()> start;
};
}
