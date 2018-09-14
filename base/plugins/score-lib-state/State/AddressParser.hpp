#pragma once
#include <State/Address.hpp>
#include <State/ValueParser.hpp>
BOOST_FUSION_ADAPT_STRUCT(State::Address, (QString, device)(QStringList, path))

BOOST_FUSION_ADAPT_STRUCT(
    ossia::destination_qualifiers,
    (ossia::destination_index, accessors)(ossia::unit_t, unit))

BOOST_FUSION_ADAPT_STRUCT(
    State::AddressAccessor,
    (State::Address, address)(ossia::destination_qualifiers, qualifiers))
BOOST_FUSION_ADAPT_STRUCT(State::Pulse, (State::Address, address))

namespace
{
/// Address parsing.
namespace qi = boost::spirit::qi;

using boost::spirit::qi::rule;

template <typename Iterator>
struct Address_parser : qi::grammar<Iterator, State::Address()>
{
  Address_parser() : Address_parser::base_type(start)
  {
    using qi::alnum;
    // OPTIMIZEME
    dev = +qi::char_(std::string(ossia::net::device_characters()));
    member_elt
        = +qi::char_(std::string(ossia::net::pattern_match_characters()));
    path %= (+("/" >> member_elt) | "/");
    start %= dev >> ":" >> path;
  }

  qi::rule<Iterator, QString()> dev;
  qi::rule<Iterator, QString()> member_elt;
  qi::rule<Iterator, QStringList()> path;
  qi::rule<Iterator, State::Address()> start;
};

template <typename Iterator>
struct AccessorList_parser : qi::grammar<Iterator, ossia::destination_index()>
{
  AccessorList_parser() : AccessorList_parser::base_type(start)
  {
    using boost::spirit::int_;
    using boost::spirit::qi::skip;
    using boost::spirit::standard::space;
    using qi::alnum;

    index %= skip(space)["[" >> int_ >> "]"];
    start %= skip(space)[+(index)];
  }

  qi::rule<Iterator, ossia::destination_index()> start;
  qi::rule<Iterator, uint8_t()> index;
};

template <typename Iterator>
struct AddressQualifiers_parser
    : qi::grammar<Iterator, ossia::destination_qualifiers()>
{
  AddressQualifiers_parser() : AddressQualifiers_parser::base_type(start)
  {
    using boost::spirit::int_;
    using boost::spirit::qi::skip;
    using boost::spirit::standard::space;
    using qi::alnum;

    unit %= boost::spirit::eoi;
    start %= "@" >> ((accessors >> -unit)
                     | ("[" >> ossia::get_unit_parser() >> "]"));
  }

  qi::rule<Iterator, ossia::destination_qualifiers()> start;
  AccessorList_parser<Iterator> accessors;
  qi::rule<Iterator, ossia::unit_t()> unit;
};

template <typename Iterator>
struct AddressAccessor_parser : qi::grammar<Iterator, State::AddressAccessor()>
{
  AddressAccessor_parser() : AddressAccessor_parser::base_type(start)
  {
    using boost::spirit::int_;
    using boost::spirit::qi::skip;
    using boost::spirit::standard::space;
    using qi::alnum;

    start %= skip(space)[address >> qualifiers];
  }

  qi::rule<Iterator, State::AddressAccessor()> start;
  Address_parser<Iterator> address;
  AddressQualifiers_parser<Iterator> qualifiers;
};
}

ossia::optional<State::Address> State::parseAddress(const QString& str)
{
  auto input = str.toStdString();
  auto f(std::begin(input)), l(std::end(input));
  auto p = std::make_unique<Address_parser<decltype(f)>>();
  try
  {
    State::Address result;
    bool ok = qi::phrase_parse(f, l, *p, qi::standard::space, result);

    if (!ok)
    {
      return {};
    }

    return result;
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

ossia::optional<State::AddressAccessor>
State::parseAddressAccessor(const QString& str)
{
  auto input = str.toStdString();
  auto f(std::begin(input)), l(std::end(input));
  auto p = std::make_unique<AddressAccessor_parser<decltype(f)>>();
  try
  {
    State::AddressAccessor result;
    bool ok = qi::phrase_parse(f, l, *p, qi::standard::space, result);

    if (ok)
    {
      return result;
    }
    else
    {
      // We try to get an address instead.
      ossia::optional<State::Address> res = State::parseAddress(str);
      if (res)
      {
        result.address = (*res);
        result.qualifiers.get().accessors.clear();

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
    // SCORE_BREAKPOINT;
    return {};
  }
  catch (...)
  {
    // SCORE_BREAKPOINT;
    return {};
  }
}
