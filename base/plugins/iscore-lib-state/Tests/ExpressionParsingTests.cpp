
/*
bool checkLeaves(const State::Expression* e)
{
    auto c = e->children(); // TODO see why this isn't a const ref return.
    if(c.isEmpty())
    {
        return e->is<State::Relation>();
    }
    else
    {
        return std::all_of(
                    c.cbegin(), c.cend(),
                    [] (auto e) {
            return checkLeaves(e);
        });
    }
}

bool validate(const State::Expression& expr)
{
    // Check that all the leaves are relations.
    return checkLeaves(&expr);
}
*/
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <QDebug>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtTest/QtTest>
#include <State/Expression.hpp>
#include <State/TraversalPath.hpp>
#include <boost/proto/operators.hpp>
#include <boost/spirit/home/qi/detail/parse_auto.hpp>
#include <boost/spirit/home/qi/operator/expect.hpp>
#include <boost/spirit/home/qi/parse.hpp>
#include <boost/spirit/home/qi/parse_attr.hpp>
#include <boost/spirit/home/support/common_terminals.hpp>
#include <boost/variant/detail/apply_visitor_unary.hpp>
#include <eggs/variant/variant.hpp>
using namespace iscore;
#include <State/ExpressionParser.cpp>
#include <State/ValueConversion.hpp>
#include <iterator>
#include <list>
#include <string>
#include <vector>

/*
void test_parse_expr()
{
    for (auto& input : std::list<std::string> {
         "(dev:/minuit <= 5) and (a:/b == 1.34);",
         "(dev:/minuit != [1, 2, 3.12, 'c']) and not (a:/b >= c:/d/e/f);"
   })
    {
        auto f(std::begin(input)), l(std::end(input));
        parser<decltype(f)> p;

        try
        {
            expr result;
            bool ok = qi::phrase_parse(f,l,p > ';',qi::space,result);

            if (!ok)
                qDebug() << "invalid input\n";
            //else
            //    qDebug() << "result: " << result << "\n";

        } catch (const qi::expectation_failure<decltype(f)>& e)
        {
            qDebug() << "expectation_failure at '" <<
QString::fromStdString(std::string(e.first, e.last)) << "'\n";
        }

        if (f!=l) qDebug() << "unparsed: '" <<
QString::fromStdString(std::string(f,l)) << "'\n";
    }

    exit(0);
}*/

// TODO move me
QDebug operator<<(QDebug dbg, const State::Address& a);
QDebug operator<<(QDebug dbg, const State::RelationMember& v);
QDebug operator<<(QDebug dbg, const ossia::expressions::comparator& v);
QDebug operator<<(QDebug dbg, const State::Relation& v);
QDebug operator<<(QDebug dbg, const State::BinaryOperator& v);
QDebug operator<<(QDebug dbg, const State::UnaryOperator& v);
QDebug operator<<(QDebug dbg, const State::ExprData& v);
QDebug operator<<(QDebug dbg, const State::Expression& v);

QDebug operator<<(QDebug dbg, const State::Address& a)
{
  dbg << a.toString();
  return dbg;
}

QDebug operator<<(QDebug dbg, const State::RelationMember& v)
{
  dbg << State::toString(v);
  return dbg;
}

QDebug operator<<(QDebug dbg, const ossia::expressions::comparator& v)
{
  switch (v)
  {
    case ossia::expressions::comparator::DIFFERENT:
      dbg << "!=";
      break;
    case ossia::expressions::comparator::EQUAL:
      dbg << "==";
      break;
    case ossia::expressions::comparator::GREATER:
      dbg << ">";
      break;
    case ossia::expressions::comparator::GREATER_EQUAL:
      dbg << ">=";
      break;
    case ossia::expressions::comparator::LOWER:
      dbg << "<";
      break;
    case ossia::expressions::comparator::LOWER_EQUAL:
      dbg << "<=";
      break;
  }
  return dbg;
}

QDebug operator<<(QDebug dbg, const State::Relation& v)
{
  dbg << v.lhs << v.op << v.rhs;
  return dbg;
}

QDebug operator<<(QDebug dbg, const State::BinaryOperator& v)
{
  switch (v)
  {
    case State::BinaryOperator::AND:
      dbg << "and";
      break;
    case State::BinaryOperator::OR:
      dbg << "or";
      break;
    case State::BinaryOperator::XOR:
      dbg << "xor";
      break;
    default:
      dbg << "none";
      break;
  }
  return dbg;
}

QDebug operator<<(QDebug dbg, const State::UnaryOperator& v)
{
  switch (v)
  {
    case State::UnaryOperator::Not:
      dbg << "not";
      break;
    default:
      dbg << "none";
      break;
  }
  return dbg;
}

QDebug operator<<(QDebug dbg, const State::ExprData& v)
{
  return dbg << v.toString();
}

QDebug operator<<(QDebug dbg, const State::Expression& v)
{
  dbg << "{";
  dbg << static_cast<const State::ExprData&>(v);
  for (auto& child : v.children())
  {
    dbg << child;
  }
  dbg << "}";
  return dbg;
}

static void debug_path(iscore::optional<State::AddressAccessor> addr ) {
  qDebug() << addr->toString();
}
static void debug_path(iscore::optional<State::Address> addr ) {
  qDebug() << addr->toString();
}
using namespace iscore;
class ExpressionParsingTests : public QObject
{
  Q_OBJECT
public:
private slots:

  void test_parse_impulse()
  {

    {
      std::string str("minuit:/device/lol impulse");

      typedef std::string::const_iterator iterator_type;
      Pulse_parser<iterator_type> parser;
      auto first = str.cbegin(), last = str.cend();
      State::Pulse p;
      bool r = parse(first, last, parser, p);

      QVERIFY(r);
    }

    {
      std::string str("{ minuit:/device/lol impulse }");

      typedef std::string::const_iterator iterator_type;
      Pulse_parser<iterator_type> parser;
      auto first = str.cbegin(), last = str.cend();
      State::Pulse p;
      bool r = parse(first, last, parser, p);

      QVERIFY(r);
    }

    {
      QString str("minuit:/device/lol impulse");

      auto expr = State::parseExpression(str);

      QVERIFY(bool(expr) == true);
    }

    {
      QString str("{ minuit:/device/lol impulse }");

      auto expr = State::parseExpression(str);

      QVERIFY(bool(expr) == true);
    }
  }

  void test_parse_array()
  {

    {
      std::string str("minuit:/device/lol@[7]");

      typedef std::string::const_iterator iterator_type;
      AddressAccessor_parser<iterator_type> parser;
      auto first = str.cbegin(), last = str.cend();
      State::AddressAccessor p;
      bool r = parse(first, last, parser, p);

      QVERIFY(r);
      QVERIFY(p.address.toString() == "minuit:/device/lol");
      QVERIFY(p.qualifiers.get().accessors.size() == 1);
      QVERIFY(p.qualifiers.get().accessors[0] == 7);
    }

    {
      std::string str("minuit:/device/lol@[7]");

      typedef std::string::const_iterator iterator_type;
      RelationMember_parser<iterator_type> parser;
      auto first = str.cbegin(), last = str.cend();
      State::RelationMember rm;
      bool r = parse(first, last, parser, rm);

      QVERIFY(r);
      QVERIFY(rm.which() == 1);
      auto& p = eggs::variants::get<State::AddressAccessor>(rm);
      QVERIFY(p.address.toString() == "minuit:/device/lol");
      QVERIFY(p.qualifiers.get().accessors.size() == 1);
      QVERIFY(p.qualifiers.get().accessors[0] == 7);
    }

    {
      QString str("{ minuit:/device/lol@[1][2] < 3.14 }");

      auto expr = State::parseExpression(str);

      QVERIFY(bool(expr) == true);
    }
  }

  void test_parse_dataspace()
  {
    {
      std::string str("minuit:/device/lol@[color.rgb]");

      typedef std::string::const_iterator iterator_type;
      AddressAccessor_parser<iterator_type> parser;
      auto first = str.cbegin(), last = str.cend();
      State::AddressAccessor p;
      bool r = parse(first, last, parser, p);

      QVERIFY(r);
      QVERIFY(p.address.toString() == "minuit:/device/lol");
      QVERIFY(p.qualifiers.get().accessors.size() == 0);
      QVERIFY(p.qualifiers.get().unit == ossia::rgb_u{});
    }

    {
      std::string str("minuit:/device/lol@[color.hsv.s]");

      typedef std::string::const_iterator iterator_type;
      AddressAccessor_parser<iterator_type> parser;
      auto first = str.cbegin(), last = str.cend();
      State::AddressAccessor p;
      bool r = parse(first, last, parser, p);

      QVERIFY(r);
      QVERIFY(p.address.toString() == "minuit:/device/lol");

      QVERIFY(p.qualifiers.get().unit == ossia::hsv_u{});

      QVERIFY(p.qualifiers.get().accessors.size() == 1);
      QVERIFY(p.qualifiers.get().accessors[0] == 1);
    }

    {
      QString str("{minuit:/device/lol@[color.rgb] < 3.14}");

      auto expr = State::parseExpression(str);

      QVERIFY(bool(expr) == true);
    }
  }

  void test_parse_addr()
  {
    using namespace qi;

    {
      std::string str("minuit:/device/lol");

      typedef std::string::const_iterator iterator_type;
      using qi::parse;
      using ascii::space;

      Address_parser<iterator_type> parser;
      auto first = str.cbegin(), last = str.cend();
      State::Address addr;
      bool r = parse(first, last, parser, addr);

      qDebug() << "parsed?" << r;
      qDebug() << addr.device;
      for (auto& elt : addr.path)
        qDebug() << elt;
    }

    {
      std::string str("minuit:/device/lol");

      typedef std::string::const_iterator iterator_type;
      RelationMember_parser<iterator_type> parser;
      auto first = str.cbegin(), last = str.cend();
      State::RelationMember rm;
      bool r = parse(first, last, parser, rm);

      QVERIFY(r);
      QVERIFY(rm.which() == 0);
      auto& p = eggs::variants::get<State::Address>(rm);
      QVERIFY(p.toString() == "minuit:/device/lol");
    }
  }

  void test_parse_value()
  {
    std::vector<std::string> str_list{
        "[1,2,3]", "[1]",  "[ 1 ]", "[ 1, 2, 3 ]", "[ 1, 2.3, 3, 'c' ]",
        "1",       "1.23", "'c'",   "\"lala\"",    "\"lala lala\""};

    for (const auto& str : str_list)
    {

      typedef std::string::const_iterator iterator_type;
      using qi::parse;

      Value_parser<iterator_type> parser;
      auto first = str.cbegin(), last = str.cend();
      State::Value val;
      bool r = parse(first, last, parser, val);

      qDebug() << str.c_str() << r << val << "                    ";
    }
  }

  void test_parse_rel_member()
  {
    std::vector<std::string> str_list{
        "minuit:/device"
        "1234"};

    for (const auto& str : str_list)
    {
      typedef std::string::const_iterator iterator_type;
      using qi::parse;

      RelationMember_parser<iterator_type> parser;
      auto first = str.cbegin(), last = str.cend();
      State::RelationMember val;
      bool r = parse(first, last, parser, val);

      qDebug() << str.c_str() << r << val.which();
    }
  }

  void test_parse_rel()
  {
    std::vector<std::string> str_list{"minuit:/device<=1234",
                                      "minuit:/device <= 1234"};

    for (const auto& str : str_list)
    {
      typedef std::string::const_iterator iterator_type;
      using qi::parse;

      Relation_parser<iterator_type> parser;
      auto first = str.cbegin(), last = str.cend();
      State::Relation val;
      bool r = parse(first, last, parser, val);

      qDebug() << str.c_str() << r << val.lhs.target<State::Address>()->path
               << State::convert::toPrettyString(
                      *val.rhs.target<State::Value>());
    }
  }

  void test_parse_expr_full()
  {
    for (auto& input : std::list<std::string>{
         "dev:/minuit != [1, 2, 3.12, 'c'];",
         "{ dev:/minuit != [1, 2, 3.12, 'c'] };",
         "a:/b >= c:/d/e/f;",
         "{ a:/b >= c:/d/e/f };",
         "{ dev:/minuit != [1, 2, 3.12, 'c']} and not { a:/b >= c:/d/e/f };",
         "{ { dev:/minuit != [1, 2, 3.12, 'c'] } and not { a:/b >= c:/d/e/f } };"
  })
    {
      auto f(std::begin(input)), l(std::end(input));
      Expression_parser<decltype(f)> p;

      try
      {
        expr_raw result;
        bool ok = qi::phrase_parse(f, l, p > ';', qi::space, result);

        if (!ok)
        {
          qDebug() << "invalid input\n";
          continue;
        }

        State::Expression e;

        Expression_builder bldr{&e};
        boost::apply_visitor(bldr, result);
        qDebug() << e;

        std::cout << std::flush;
      }
      catch (const qi::expectation_failure<decltype(f)>& e)
      {
        using namespace std::literals;
        std::cerr << input << std::string(" : expectation_failure at '")
                  << std::string(e.first, e.last) << std::string("'\n");
        QVERIFY(false);
      }

      // if (f!=l) std::cerr << "unparsed: '" << std::string(f,l) << "'\n";
    }

    // return 0;
  }

  void test_address_dot_in_instances()
  {
    debug_path(State::parseAddressAccessor("myapp:/score/color.1@[color.rgb.r]"));
    debug_path(State::parseAddressAccessor("myapp:/score/color.1"));
    debug_path(State::parseAddressAccessor("myapp:/score/color@[color.rgb.r]"));
    debug_path(State::parseAddressAccessor("myapp:/score/color.1@[color.rgb]"));

    debug_path(State::parseAddress("myapp:/score/color.1@[color.rgb.r]"));
    debug_path(State::parseAddress("myapp:/score/color.1"));
    debug_path(State::parseAddress("myapp:/score/color@[color.rgb.r]"));
    debug_path(State::parseAddress("myapp:/score/color.1@[color.rgb]"));
  }

  void test_parse_random()
  {
    using namespace std::literals;
    QVERIFY(bool(State::parseAddressAccessor("myapp:/score/color.1@[color.rgb.r]")));
    QVERIFY(bool(State::parseExpression("{ myapp:/score > 2}"s)));
    QVERIFY(bool(State::parseExpression("{2 > myapp:/stagescore }"s)));
    QVERIFY(
        bool(State::parseExpression("{ myapp:/score > myapp:/stagescore }"s)));
    QVERIFY(
        bool(State::parseExpression("{ myapp:/score >= myapp:/stagescore }"s)));
    QVERIFY(
        bool(State::parseExpression("{ my_app:/score > my_app:/stagescore }"s)));
    QVERIFY(
        bool(State::parseExpression("{ my_app:/score > my_app:/stage_score }"s)));
    QVERIFY(
        bool(State::parseExpression("{ my_app:/score > my_app:/stage_score }"s)));
    QVERIFY(bool(State::parseExpression("{ { A:/B > c:/D } and { e:/f > g:/h } }"s)));
  }


  void test_parse_patternmatch()
  {
    using namespace std::literals;

    QVERIFY(bool(State::TraversalPath::make_path("myapp:/score")));
    QVERIFY(bool(State::TraversalPath::make_path("myapp:/score.")));
    QVERIFY(bool(State::TraversalPath::make_path("myapp:/score.*")));
    QVERIFY(bool(State::TraversalPath::make_path("../score/blop")));
    QVERIFY(bool(State::TraversalPath::make_path("myapp:/score.*/[a-z]*/{blurg}")));
    QVERIFY(bool(State::TraversalPath::make_path("//score")));
    QVERIFY(bool(State::TraversalPath::make_path("//score/blop")));
  }
};

QTEST_APPLESS_MAIN(ExpressionParsingTests)
#include "ExpressionParsingTests.moc"
#include <State/Address.hpp>
#include <State/Relation.hpp>
#include <State/Value.hpp>
