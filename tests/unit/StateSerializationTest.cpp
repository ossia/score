#include <State/Address.hpp>
#include <State/Domain.hpp>
#include <State/Expression.hpp>
#include <State/Message.hpp>
#include <State/Unit.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <core/application/MockApplication.hpp>

#include <ossia/network/common/destination_qualifiers.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/network/domain/domain.hpp>

#include <catch2/catch_test_macros.hpp>

#include <csignal>
#include <random>

namespace
{
// Must outlive every test: serializers dereference AppComponents().
static const score::testing::MockApplication g_mock_app;

static const int g_ignore_sigtrap = [] {
  std::signal(SIGTRAP, SIG_IGN);
  return 0;
}();

template <typename T>
T dsRoundTrip(const T& t)
{
  const QByteArray bytes = score::marshall<DataStream>(t);
  REQUIRE(!bytes.isEmpty());
  return score::unmarshall<T>(bytes);
}

template <typename T>
T jsonRoundTrip(const T& t)
{
  const QByteArray bytes = toJson(t);
  REQUIRE(!bytes.isEmpty());
  return fromJson<T>(bytes);
}

template <typename T>
bool gracefulUnmarshall(const QByteArray& bytes)
{
  try
  {
    (void)score::unmarshall<T>(bytes);
    return true;
  }
  catch(const std::exception&)
  {
    return true;
  }
  catch(...)
  {
    return true;
  }
}
}

TEST_CASE("AddressAccessor parses indices and units", "[state][address]")
{
  SECTION("plain accessor index")
  {
    const auto acc = State::parseAddressAccessor("foo:/bar@[1]");
    REQUIRE(acc.has_value());
    CHECK(acc->address.device == "foo");
    REQUIRE(acc->address.path.size() == 1);
    CHECK(acc->address.path[0] == "bar");

    const ossia::destination_qualifiers& q = acc->qualifiers.get();
    REQUIRE(q.accessors.size() == 1);
    CHECK(q.accessors[0] == 1);
    CHECK(!q.unit);
  }

  SECTION("unit qualifier")
  {
    const auto acc = State::parseAddressAccessor("dev:/light@[color.rgb]");
    REQUIRE(acc.has_value());
    const ossia::destination_qualifiers& q = acc->qualifiers.get();
    REQUIRE(!!q.unit);
    CHECK(q.unit == ossia::parse_pretty_unit("color.rgb"));
  }

  SECTION("unit + component accessor")
  {
    const auto acc = State::parseAddressAccessor("dev:/light@[color.rgb.r]");
    REQUIRE(acc.has_value());
    const ossia::destination_qualifiers& q = acc->qualifiers.get();
    CHECK(!!q.unit);
    // Selecting the .r component turns into an accessor index on the unit
    CHECK(!q.accessors.empty());
  }

  SECTION("string round-trip")
  {
    for(const char* str :
        {"foo:/bar@[1]", "dev:/light@[color.rgb]", "dev:/light@[color.rgb.r]",
         "a:/b/c/d@[2]"})
    {
      const auto acc = State::parseAddressAccessor(str);
      REQUIRE(acc.has_value());
      const auto reparsed = State::parseAddressAccessor(acc->toString());
      REQUIRE(reparsed.has_value());
      CHECK(*reparsed == *acc);
    }
  }
}

TEST_CASE("Address DataStream round-trip", "[state][address][serialization]")
{
  SECTION("regular address")
  {
    const auto addr = State::Address::fromString("aDevice:/some/node");
    REQUIRE(addr.has_value());
    const auto back = dsRoundTrip(*addr);
    CHECK(back == *addr);
    CHECK(back.device == "aDevice");
    CHECK(back.path == QStringList{QStringLiteral("some"), QStringLiteral("node")});
  }

  SECTION("root (device only) address")
  {
    State::Address root{"dev", {}};
    const auto back = dsRoundTrip(root);
    CHECK(back == root);
    CHECK(back.path.isEmpty());
  }

  SECTION("unicode fragments")
  {
    State::Address addr{QStringLiteral("装置"), {QStringLiteral("était")}};
    const auto back = dsRoundTrip(addr);
    CHECK(back == addr);
  }
}

TEST_CASE("Address JSON round-trip", "[state][address][serialization]")
{
  const auto addr = State::Address::fromString("aDevice:/some/node");
  REQUIRE(addr.has_value());
  const auto back = jsonRoundTrip(*addr);
  CHECK(back == *addr);
}

TEST_CASE("AddressAccessor DataStream + JSON round-trip", "[state][address][serialization]")
{
  for(const char* str :
      {"foo:/bar@[1]", "dev:/light@[color.rgb]", "dev:/light@[color.rgb.r]"})
  {
    const auto acc = State::parseAddressAccessor(str);
    REQUIRE(acc.has_value());

    const auto ds = dsRoundTrip(*acc);
    CHECK(ds == *acc);

    const auto js = jsonRoundTrip(*acc);
    CHECK(js == *acc);
  }
}

static std::vector<ossia::value> testValues()
{
  return {
      ossia::value{ossia::impulse{}},
      ossia::value{int32_t{-42}},
      ossia::value{2.5f},
      ossia::value{true},
      ossia::value{false},
      ossia::value{std::string{"hello world"}},
      ossia::value{std::string{"héllo wörld ⇒ 日本"}},
      ossia::value{ossia::vec2f{0.5f, -1.5f}},
      ossia::value{ossia::vec3f{0.25f, 0.5f, 0.75f}},
      ossia::value{ossia::vec4f{1.f, 2.f, 3.f, 4.f}},
      ossia::value{std::vector<ossia::value>{
          int32_t{1}, 2.5f, std::string{"x"},
          std::vector<ossia::value>{int32_t{5}, int32_t{6}}}},
      ossia::value{ossia::value_map_type{{"a", int32_t{1}}, {"b", 0.5f}}},
  };
}

TEST_CASE("Message DataStream round-trip over all value types", "[state][message][serialization]")
{
  const auto acc = State::parseAddressAccessor("dev:/foo/bar@[1]");
  REQUIRE(acc.has_value());

  for(const auto& v : testValues())
  {
    State::Message msg{*acc, v};
    const auto back = dsRoundTrip(msg);
    CHECK(back == msg);
    CHECK(back.value == v);
    CHECK(back.address == *acc);
  }
}

TEST_CASE("Message JSON round-trip over all value types", "[state][message][serialization]")
{
  const auto acc = State::parseAddressAccessor("dev:/foo/bar@[1]");
  REQUIRE(acc.has_value());

  for(const auto& v : testValues())
  {
    State::Message msg{*acc, v};
    const auto back = jsonRoundTrip(msg);
    CHECK(back == msg);
    CHECK(back.value == v);
  }
}

TEST_CASE("MessageList DataStream round-trip", "[state][message][serialization]")
{
  State::MessageList list;
  const auto a1 = State::parseAddressAccessor("dev:/a");
  const auto a2 = State::parseAddressAccessor("other:/b/c@[2]");
  REQUIRE(a1.has_value());
  REQUIRE(a2.has_value());
  list.push_back(State::Message{*a1, ossia::value{int32_t{7}}});
  list.push_back(State::Message{*a2, ossia::value{std::string{"str"}}});

  const auto back = dsRoundTrip(list);
  REQUIRE(back.size() == 2);
  CHECK(back[0] == list[0]);
  CHECK(back[1] == list[1]);
}

TEST_CASE("Domain DataStream round-trip", "[state][domain][serialization]")
{
  SECTION("float min/max domain")
  {
    State::Domain d{ossia::make_domain(0.f, 127.f)};
    const auto back = dsRoundTrip(d);
    CHECK(back == d);
    CHECK(back.get().convert_min<float>() == 0.f);
    CHECK(back.get().convert_max<float>() == 127.f);
  }

  SECTION("int min/max domain")
  {
    State::Domain d{ossia::make_domain(int32_t{-5}, int32_t{5})};
    const auto back = dsRoundTrip(d);
    CHECK(back == d);
    CHECK(back.get().convert_min<int>() == -5);
    CHECK(back.get().convert_max<int>() == 5);
  }

  SECTION("empty domain")
  {
    State::Domain d;
    const auto back = dsRoundTrip(d);
    CHECK(back == d);
  }
}

TEST_CASE("Value to string and back", "[state][value][conversion]")
{
  SECTION("int")
  {
    const ossia::value v{int32_t{123}};
    const QString s = State::convert::toPrettyString(v);
    CHECK(s == "123");
    const auto parsed = State::parseValue(s.toStdString());
    REQUIRE(parsed.has_value());
    CHECK(*parsed == v);
  }

  SECTION("float")
  {
    const ossia::value v{2.5f};
    const auto parsed = State::parseValue(State::convert::toPrettyString(v).toStdString());
    REQUIRE(parsed.has_value());
    CHECK(*parsed == v);
  }

  SECTION("quoted string")
  {
    const ossia::value v{std::string{"foo bar"}};
    const QString s = State::convert::toPrettyString(v);
    CHECK(s == "\"foo bar\"");
    const auto parsed = State::parseValue(s.toStdString());
    REQUIRE(parsed.has_value());
    CHECK(*parsed == v);
  }

  SECTION("list")
  {
    const ossia::value v{std::vector<ossia::value>{int32_t{1}, int32_t{2}, int32_t{3}}};
    const QString s = State::convert::toPrettyString(v);
    const auto parsed = State::parseValue(s.toStdString());
    REQUIRE(parsed.has_value());
    CHECK(*parsed == v);
  }

  SECTION("bool")
  {
    CHECK(State::convert::toPrettyString(ossia::value{true}) == "true");
    CHECK(State::convert::toPrettyString(ossia::value{false}) == "false");
    const auto parsed = State::parseValue("true");
    REQUIRE(parsed.has_value());
    CHECK(*parsed == ossia::value{true});
  }

  SECTION("garbage does not parse")
  {
    CHECK(!State::parseValue("@#!garbage^&").has_value());
  }

  SECTION("typed conversions")
  {
    CHECK(State::convert::value<int>(ossia::value{2.7f}) == 2);
    CHECK(State::convert::value<float>(ossia::value{int32_t{3}}) == 3.f);
    CHECK(State::convert::value<QString>(ossia::value{std::string{"x"}}) == "x");
  }
}

TEST_CASE("Expression parse and string round-trip", "[state][expression]")
{
  const auto expr = State::parseExpression(QStringLiteral("{ %dev:/foo% == 5 }"));
  REQUIRE(expr.has_value());

  const QString s1 = expr->toString();
  const auto reparsed = State::parseExpression(s1);
  REQUIRE(reparsed.has_value());
  CHECK(reparsed->toString() == s1);

  CHECK(!State::parseExpression(QStringLiteral("((((")).has_value());
}

TEST_CASE("Truncated DataStream buffers are handled gracefully", "[state][serialization][fuzz]")
{
  const auto acc = State::parseAddressAccessor("dev:/foo/bar@[1]");
  REQUIRE(acc.has_value());
  State::Message msg{
      *acc, ossia::value{std::vector<ossia::value>{
                int32_t{1}, std::string{"payload"}, ossia::vec3f{1, 2, 3}}}};

  const QByteArray full = score::marshall<DataStream>(msg);
  REQUIRE(full.size() > 8);

  for(int len = 0; len < full.size(); ++len)
  {
    CHECK(gracefulUnmarshall<State::Message>(full.left(len)));
  }

  // The full buffer still round-trips.
  CHECK(score::unmarshall<State::Message>(full) == msg);

  // Same exercise for AddressAccessor.
  const QByteArray accBytes = score::marshall<DataStream>(*acc);
  for(int len = 0; len < accBytes.size(); ++len)
  {
    CHECK(gracefulUnmarshall<State::AddressAccessor>(accBytes.left(len)));
  }
}

TEST_CASE("Random garbage buffers do not crash Address deserialization", "[state][serialization][fuzz]")
{
  std::mt19937 rng{20260717};
  std::uniform_int_distribution<int> byteDist{0, 255};
  std::uniform_int_distribution<int> sizeDist{1, 64};

  for(int iter = 0; iter < 500; ++iter)
  {
    QByteArray buf;
    const int n = sizeDist(rng);
    buf.reserve(n);
    for(int i = 0; i < n; ++i)
      buf.append(static_cast<char>(byteDist(rng)));

    CHECK(gracefulUnmarshall<State::Address>(buf));
  }
}
