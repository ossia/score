// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <State/ExpressionParser.hpp>

std::optional<State::Expression> State::parseExpression(const std::string& input)
{
  auto f(std::begin(input)), l(std::end(input));
  auto p = std::make_unique<Expression_parser<decltype(f)>>();
  try
  {
    expr_raw result;
    bool ok = qi::phrase_parse(f, l, *p, qi::standard::space, result);

    if(!ok)
    {
      return {};
    }

    State::Expression e;

    Expression_builder bldr{&e};
    boost::apply_visitor(bldr, result);

    return e;
  }
  catch(const qi::expectation_failure<decltype(f)>& e)
  {
    // SCORE_BREAKPOINT;
    return {};
  }
  catch(...)
  {
    // SCORE_BREAKPOINT;
    return {};
  }
}

std::optional<State::Expression> State::parseExpression(const QString& str)
{
  return parseExpression(str.toStdString());
}
