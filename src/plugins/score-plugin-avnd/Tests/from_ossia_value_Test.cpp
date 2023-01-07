// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

// #include <ossia/network/common/complex_type.hpp>
// #include <ossia/network/generic/generic_device.hpp>
// #include <ossia/network/generic/generic_node.hpp>
// #include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/value/format_value.hpp>
#include <ossia/network/value/value.hpp>

#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/to_value.hpp>
#include <catch2/catch_all.hpp>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace ossia
{
std::ostream& operator<<(std::ostream& s, const ossia::value& v)
{
  return s << ossia::value_to_pretty_string(v);
}
}

namespace std
{
template <typename... Args>
std::ostream& operator<<(std::ostream& s, const std::variant<Args...>& v)
{
  s << v.index() << ": ";
  std::visit([&](const auto& m) { s << fmt::to_string(m); }, v);
  return s;
}
}

/*
TEST_CASE("map conversions", "aggregate")
{
  GIVEN("a device tree")
  {
    ossia::net::generic_device dev;
    auto p1 = ossia::create_parameter(dev.get_root_node(), "/foo/bar", "float");
    auto p2 = ossia::create_parameter(dev.get_root_node(), "/foo/baz", "container");
    auto p3 = ossia::create_parameter(dev.get_root_node(), "/foo/baz/bux", "float");

    auto foo = dev.get_root_node().children()[0].get();

    p1->set_value(1.3f);
    p3->set_value(54.3f);

    GIVEN("an aggregate")
    {

      struct
      {
        float bar = 0;
        struct
        {
          float bux = 0;
        } baz;
      } aggregate;

      WHEN("from_value is called")
      {
        ossia::value v = std::vector<ossia::value>{123, 4.56f, "hello world", true};
        oscr::from_ossia_value(v, aggregate);
        THEN("the conversion is correct")
        {
          REQUIRE(aggregate.bar == 1.3f);
          REQUIRE(aggregate.baz.bux == 4.56f);
        }
      }
    }
  }
}
*/

TEST_CASE("type conversions", "aggregate")
{
  GIVEN("an aggregate")
  {
    struct
    {
      int a = 0;
      float b = 0.f;
      std::string c = "";
      bool d = false;
    } aggregate;

    WHEN("from_value is called")
    {
      ossia::value v = std::vector<ossia::value>{123, 4.56f, "hello world", true};
      oscr::from_ossia_value(v, aggregate);
      THEN("the conversion is correct")
      {
        REQUIRE(aggregate.a == 123);
        REQUIRE(aggregate.b == 4.56f);
        REQUIRE(aggregate.c == "hello world");
        REQUIRE(aggregate.d == true);
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(aggregate);
        REQUIRE(v == roundtrip);
      }
    }
  }

  GIVEN("a vec2-ish aggregate")
  {
    struct
    {
      float a = 0.f;
      float b = 0.f;
    } aggregate;

    WHEN("from_value is called")
    {
      ossia::value v = ossia::vec2f{123.f, 4.56f};
      oscr::from_ossia_value(v, aggregate);
      THEN("the conversion is correct")
      {
        REQUIRE(aggregate.a == 123.f);
        REQUIRE(aggregate.b == 4.56f);
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(aggregate);
        REQUIRE(v == roundtrip);
      }
    }
  }

  GIVEN("a less vec2-ish aggregate")
  {
    struct
    {
      int a = 0;
      float b = 0.f;
    } aggregate;

    WHEN("from_value is called")
    {
      ossia::value v = ossia::vec2f{123.f, 4.56f};
      oscr::from_ossia_value(v, aggregate);
      THEN("the conversion is correct")
      {
        REQUIRE(aggregate.a == 123);
        REQUIRE(aggregate.b == 4.56f);
      }

      THEN("the round-trip is for a meaningful type for the aggregate")
      {
        ossia::value roundtrip = oscr::to_ossia_value(aggregate);
        REQUIRE(v == ossia::vec2f{123.f, 4.56f});
      }
    }
  }

  GIVEN("a recursive aggregate")
  {
    struct
    {
      struct
      {
        int a = 0;
      } a;
      struct
      {
        float b = 0.f;
      } b;
    } aggregate;

    WHEN("from_value is called")
    {
      ossia::value v = std::vector<ossia::value>{
          std::vector<ossia::value>{123}, std::vector<ossia::value>{4.56f}};
      oscr::from_ossia_value(v, aggregate);
      THEN("the conversion is correct")
      {
        REQUIRE(aggregate.a.a == 123);
        REQUIRE(aggregate.b.b == 4.56f);
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(aggregate);
        REQUIRE(v == roundtrip);
      }
    }
  }

  GIVEN("a list of aggregates")
  {
    struct aggregate
    {
      int a = 0;
      float b = 0.f;
    };

    std::vector<aggregate> aggs;

    WHEN("from_value is called")
    {
      ossia::value v = std::vector<ossia::value>{
          std::vector<ossia::value>{123, 4.56f}, std::vector<ossia::value>{789, 10.0f},
          ossia::vec2f{11.4f, 45.6f}};
      oscr::from_ossia_value(v, aggs);
      THEN("the conversion is correct")
      {
        REQUIRE(aggs.size() == 3);
        REQUIRE(aggs[0].a == 123);
        REQUIRE(aggs[0].b == 4.56f);
        REQUIRE(aggs[1].a == 789);
        REQUIRE(aggs[1].b == 10.0f);
        REQUIRE(aggs[2].a == 11);
        REQUIRE(aggs[2].b == 45.6f);
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(aggs);
        ossia::value expected = std::vector<ossia::value>{
            std::vector<ossia::value>{123, 4.56f}, std::vector<ossia::value>{789, 10.0f},
            std::vector<ossia::value>{
                11, 45.6f}}; // we obviously loose the fractional part of 11.4f
        REQUIRE(expected == roundtrip);
      }
    }
  }
}

TEST_CASE("list conversions", "list")
{
  using vec = std::vector<ossia::value>;
  GIVEN("a list of int")
  {
    std::vector<int> list;
    WHEN("from_value is called")
    {
      ossia::value v = vec{123, 4.56f, 7, 8, true};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(list == std::vector<int>{123, 4, 7, 8, 1});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        ossia::value expected = std::vector<ossia::value>{123, 4, 7, 8, 1};
        REQUIRE(expected == roundtrip);
      }
    }
  }

  GIVEN("a list of float")
  {
    std::vector<float> list;
    WHEN("from_value is called")
    {
      ossia::value v = vec{123, 4.56f, 7, 8, true};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(list == std::vector<float>{123., 4.56f, 7., 8., 1.});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        REQUIRE(v == roundtrip);
      }
    }
  }

  GIVEN("a list of bool")
  {
    std::vector<bool> list;
    WHEN("from_value is called")
    {
      ossia::value v = vec{123, 4.56f, 7, 8, true, false, 0, 0.f};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(
            list
            == std::vector<bool>{true, true, true, true, true, false, false, false});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        ossia::value expected = std::vector<ossia::value>{true, true,  true,  true,
                                                          true, false, false, false};
        REQUIRE(expected == roundtrip);
      }
    }
  }

  GIVEN("a list of list of int")
  {
    std::vector<std::vector<int>> list;
    WHEN("from_value is called")
    {
      ossia::value v = vec{vec{3, 6, 7}, vec{1, 4, 5, 2}};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(list == std::vector<std::vector<int>>{{3, 6, 7}, {1, 4, 5, 2}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        REQUIRE(v == roundtrip);
      }
    }
  }

  GIVEN("a list of list of aggregate of ints")
  {
    struct pair
    {
      int a, b;

      bool operator==(const pair& other) const noexcept = default;
    };

    std::vector<std::vector<pair>> list;
    WHEN("from_value is called with std::vector<ossia::values>")
    {
      ossia::value v = vec{vec{vec{3, 6}, vec{7, 4}}, vec{vec{1, 4}, vec{5, 2}}};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(
            list
            == std::vector<std::vector<pair>>{
                std::vector<pair>{pair{3, 6}, pair{7, 4}},
                std::vector<pair>{pair{1, 4}, pair{5, 2}}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        REQUIRE(v == roundtrip);
      }
    }

    WHEN("from_value is called with vec2fs")
    {
      ossia::value v
          = vec{vec{ossia::vec2f{3, 6}, vec{7, 4}}, vec{vec{1, 4}, ossia::vec2f{5, 2}}};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(
            list
            == std::vector<std::vector<pair>>{
                std::vector<pair>{pair{3, 6}, pair{7, 4}},
                std::vector<pair>{pair{1, 4}, pair{5, 2}}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        // Everything gets std::vector'd.. maybe not that good of an idea ?
        ossia::value expected
            = vec{vec{vec{3, 6}, vec{7, 4}}, vec{vec{1, 4}, vec{5, 2}}};
        REQUIRE(expected == roundtrip);
      }
    }
  }
  GIVEN("a list of list of aggregate of floats")
  {
    struct pair
    {
      float a, b;

      bool operator==(const pair& other) const noexcept = default;
    };

    std::vector<std::vector<pair>> list;
    WHEN("from_value is called with std::vector<ossia::values>")
    {
      ossia::value v = vec{vec{vec{3, 6}, vec{7, 4}}, vec{vec{1, 4}, vec{5, 2}}};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(
            list
            == std::vector<std::vector<pair>>{
                std::vector<pair>{pair{3, 6}, pair{7, 4}},
                std::vector<pair>{pair{1, 4}, pair{5, 2}}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        ossia::value expected = vec{
            vec{ossia::vec2f{3, 6}, ossia::vec2f{7, 4}},
            vec{ossia::vec2f{1, 4}, ossia::vec2f{5, 2}}};
        REQUIRE(expected == roundtrip);
      }
    }

    WHEN("from_value is called with vec2fs")
    {
      ossia::value v
          = vec{vec{ossia::vec2f{3, 6}, vec{7, 4}}, vec{vec{1, 4}, ossia::vec2f{5, 2}}};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(
            list
            == std::vector<std::vector<pair>>{
                std::vector<pair>{pair{3, 6}, pair{7, 4}},
                std::vector<pair>{pair{1, 4}, pair{5, 2}}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        ossia::value expected = vec{
            vec{ossia::vec2f{3, 6}, ossia::vec2f{7, 4}},
            vec{ossia::vec2f{1, 4}, ossia::vec2f{5, 2}}};
        REQUIRE(expected == roundtrip);
      }
    }
  }
}

TEST_CASE("map conversions", "map")
{
  using map = ossia::value_map_type;
  GIVEN("a map of string -> int")
  {
    std::map<std::string, int> list;
    WHEN("from_value is called")
    {
      ossia::value v = map{{"foo", 123}, {"bar", 4.56f}};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(list == std::map<std::string, int>{{"foo", 123}, {"bar", 4}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        // The passage through std::map sorted it
        ossia::value expected = ossia::value_map_type{{"bar", 4}, {"foo", 123}};
        REQUIRE(roundtrip == expected);
      }
    }
  }

  GIVEN("a map of string -> string")
  {
    std::map<std::string, std::string> list;
    WHEN("from_value is called")
    {
      ossia::value v = map{{"foo", "123"}, {"bar", "hello"}};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(
            list
            == std::map<std::string, std::string>{{"foo", "123"}, {"bar", "hello"}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        // The passage through std::map sorted it
        ossia::value expected = ossia::value_map_type{{"bar", "hello"}, {"foo", "123"}};
        REQUIRE(roundtrip == expected);
      }
    }
  }

  GIVEN("a map of string -> int vector")
  {
    std::map<std::string, std::vector<int>> list;
    WHEN("from_value is called")
    {
      ossia::value v = map{
          {"foo", std::vector<ossia::value>{1, 3, 5}},
          {"bar", std::vector<ossia::value>{3.5, 10, -1}}};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(
            list
            == std::map<std::string, std::vector<int>>{
                {"foo", {1, 3, 5}}, {"bar", {3, 10, -1}}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        // The passage through std::map sorted it
        ossia::value expected = ossia::value_map_type{
            {"bar", std::vector<ossia::value>{3, 10, -1}},
            {"foo", std::vector<ossia::value>{1, 3, 5}}};
        REQUIRE(roundtrip == expected);
      }
    }
  }

  GIVEN("a map of string -> aggregate ")
  {
    struct aggregate
    {
      int a = 0;
      float b = 0.f;
      std::string c = "";
      bool d = false;
      bool operator==(const aggregate&) const noexcept = default;
    };
    std::map<std::string, aggregate> list;
    WHEN("from_value is called")
    {
      ossia::value v = map{
          {"foo", std::vector<ossia::value>{1, 3.54f, "foo", false}},
          {"bar", std::vector<ossia::value>{10, 3.4f, "toto", true}}};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(
            list
            == std::map<std::string, aggregate>{
                {"foo", {1, 3.54f, "foo", false}}, {"bar", {10, 3.4f, "toto", true}}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        // The passage through std::map sorted it
        ossia::value expected = ossia::value_map_type{
            {"bar", std::vector<ossia::value>{10, 3.4f, "toto", true}},
            {"foo", std::vector<ossia::value>{1, 3.54f, "foo", false}}};
        REQUIRE(roundtrip == expected);
      }
    }
  }

  GIVEN("a struct of struct that looks like a map -> aggregate ")
  {
    struct aggregate
    {
      int a = 0;
      float b = 0.f;
      std::string c = "";
      bool d = false;
      bool operator==(const aggregate&) const noexcept = default;
    };
    struct list
    {
      aggregate a;
      aggregate b;
      bool operator==(const list&) const noexcept = default;
    } l;
    WHEN("from_value is called")
    {
      ossia::value v = map{
          {"foo", std::vector<ossia::value>{1, 3.54f, "foo", false}},
          {"bar", std::vector<ossia::value>{10, 3.4f, "toto", true}}};
      oscr::from_ossia_value(v, l);

      THEN("the conversion is correct")
      {
        REQUIRE(l == list{{1, 3.54f, "foo", false}, {10, 3.4f, "toto", true}});
      }

      THEN("this at least maps to a list")
      {
        ossia::value roundtrip = oscr::to_ossia_value(l);
        // The passage through std::map sorted it
        ossia::value expected = std::vector<ossia::value>{
            std::vector<ossia::value>{1, 3.54f, "foo", false},
            std::vector<ossia::value>{10, 3.4f, "toto", true}};
        REQUIRE(roundtrip == expected);
      }
    }
  }
}

TEST_CASE("variant conversions", "variant")
{
  GIVEN("a variant of string/int")
  {
    using variant_type = std::variant<int, std::string>;
    variant_type var;

    WHEN("from_value is called with int")
    {
      ossia::value v = 123;
      oscr::from_ossia_value(v, var);
      THEN("the conversion is correct")
      {
        REQUIRE(var == variant_type{123});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(var);
        REQUIRE(v == roundtrip);
      }
    }

    WHEN("from_value is called with string")
    {
      ossia::value v = "foo";
      oscr::from_ossia_value(v, var);
      THEN("the conversion is correct")
      {
        REQUIRE(var == variant_type{"foo"});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(var);
        REQUIRE(v == roundtrip);
      }
    }
  }

  GIVEN("a variant of std::vector")
  {
    using variant_type = std::variant<std::vector<int>, std::vector<float>>;
    variant_type var;

    WHEN("from_value is called with vec<int>")
    {
      ossia::value v = std::vector<ossia::value>{123, 456, 7, 8, 9};
      oscr::from_ossia_value(v, var);
      THEN("the conversion is correct")
      {
        REQUIRE(var == variant_type{std::vector<int>{123, 456, 7, 8, 9}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(var);
        REQUIRE(v == roundtrip);
      }
    }

    WHEN("from_value is called with vec<float>")
    {
      ossia::value v = std::vector<ossia::value>{123.f, 456.f, 7.f, 8.f, 9.f};
      oscr::from_ossia_value(v, var);
      THEN("the conversion is correct")
      {
        REQUIRE(var == variant_type{std::vector<float>{123.f, 456.f, 7.f, 8.f, 9.f}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(var);
        REQUIRE(v == roundtrip);
      }
    }
  }

  GIVEN("a variant that looks like an ossia::value")
  {
    /* FIXME
    struct my_variant;
    using variant_type = std::variant<int, float, bool, std::vector<my_variant>>;
    struct my_variant : variant_type
    {
      using variant::variant;

      bool operator==(const my_variant& other) const noexcept = default;
    };

    variant_type var;

    WHEN("from_value is called with vec<int>")
    {
      ossia::value v = std::vector<ossia::value>{123, 456, 7, 8, 9};
      oscr::from_ossia_value(v, var);
      THEN("the conversion is correct")
      {
        REQUIRE(var == variant_type{std::vector<my_variant>{123, 456, 7, 8, 9}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(var);
        REQUIRE(v == roundtrip);
      }
    }
  */
  }
}

namespace jk::config
{
template <typename... Args>
using variant = std::variant<Args...>;
template <typename... Args>
using vector = std::vector<Args...>;
template <typename... Args>
using map = std::map<Args...>;

using string = std::string;
jk::config::string foo();
}
namespace jk
{
struct value;
using string_type = config::string;
using list_type = config::vector<value>;
using map_type = config::map<string_type, value>;
using variant = config::variant<int64_t, double, bool, string_type, list_type, map_type>;

struct value
{
  using wrapped_type = variant;
  variant v;

  value() = default;
  value(const value&) = default;
  value(value&&) noexcept = default;
  value& operator=(const value&) = default;
  value& operator=(value&&) noexcept = default;

  value(const variant& v)
      : v{v}
  {
  }
  value(variant&& v) noexcept
      : v{std::move(v)}
  {
  }

  value(list_type&& v) noexcept
      : v{std::move(v)}
  {
  }
  value(map_type&& v) noexcept
      : v{std::move(v)}
  {
  }
  value(int64_t v) noexcept
      : v{std::move(v)}
  {
  }
  value(int v) noexcept
      : v{int64_t(v)}
  {
  }
  value(float v) noexcept
      : v{(double)std::move(v)}
  {
  }
  value(double v) noexcept
      : v{std::move(v)}
  {
  }
  value(bool v) noexcept
      : v{std::move(v)}
  {
  }
  value(string_type&& v) noexcept
      : v{std::move(v)}
  {
  }
  value(const char* v) noexcept
      : v{string_type{v}}
  {
  }

  value& operator=(list_type&& v)
  {
    this->v = std::move(v);
    return *this;
  }
  value& operator=(map_type&& v)
  {
    this->v = std::move(v);
    return *this;
  }
  value& operator=(int64_t v)
  {
    this->v = std::move(v);
    return *this;
  }
  value& operator=(int v)
  {
    this->v = std::move(v);
    return *this;
  }
  value& operator=(float v)
  {
    this->v = std::move(v);
    return *this;
  }
  value& operator=(double v)
  {
    this->v = std::move(v);
    return *this;
  }
  value& operator=(bool v)
  {
    this->v = std::move(v);
    return *this;
  }
  value& operator=(string_type&& v)
  {
    this->v = std::move(v);
    return *this;
  }
  value& operator=(const char* v)
  {
    this->v = string_type{v};
    return *this;
  }

  auto& operator=(variant&& other) noexcept
  {
    v = std::move(other);
    return *this;
  }
  operator variant&() noexcept { return v; }
  operator const variant&() const noexcept { return v; }
  operator variant&&() && noexcept { return std::move(v); }

  bool operator==(const value& other) const noexcept = default;
};

}

TEST_CASE("wrapped_type conversions", "wrapped_type")
{
  using ovec = std::vector<ossia::value>;
  using jvec = jk::list_type;
  GIVEN("a int")
  {
    jk::value list;
    WHEN("from_value is called")
    {
      ossia::value v = 123;
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(list == jk::value{123});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        REQUIRE(v == roundtrip);
      }
    }
  }

  GIVEN("a list of int")
  {
    jk::value list;
    WHEN("from_value is called")
    {
      ossia::value v = ovec{3, 6, 7};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(list == jk::value{jvec{3, 6, 7}});
      }
      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        REQUIRE(v == roundtrip);
      }
    }
  }

  GIVEN("a mixed list ")
  {
    jk::value list;
    WHEN("from_value is called")
    {
      ossia::value v = ovec{3, 6, 7.1f, "foo"};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(list == jk::value{jvec{3, 6, 7.1f, "foo"}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        REQUIRE(v == roundtrip);
      }
    }
  }

  GIVEN("a list of list of int")
  {
    jk::value list;
    WHEN("from_value is called")
    {
      ossia::value v = ovec{ovec{3, 6, 7}, ovec{1, 4, 5, 2}};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(list == jk::value{jvec{jvec{3, 6, 7}, jvec{1, 4, 5, 2}}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        REQUIRE(v == roundtrip);
      }
    }
  }
}

TEST_CASE("wrapped_type map conversions", "wrapped_type")
{
  using omap = ossia::value_map_type;
  using olst = std::vector<ossia::value>;
  using jmap = jk::map_type;
  using jlst = jk::list_type;
  GIVEN("a map")
  {
    jk::value list;
    WHEN("from_value is called")
    {
      ossia::value v = omap{{"foo", 123}, {"bar", 456}};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(list == jk::value{jmap{{"foo", 123}, {"bar", 456}}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        REQUIRE(v == roundtrip);
      }
    }
  }
  GIVEN("a map of lists")
  {
    jk::value list;
    WHEN("from_value is called")
    {
      ossia::value v = omap{{"foo", olst{123, "foo"}}, {"bar", olst{456, "x", 7.89f}}};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(
            list
            == jk::value{
                jmap{{"foo", jlst{123, "foo"}}, {"bar", jlst{456, "x", 7.89f}}}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        REQUIRE(v == roundtrip);
      }
    }
  }
  GIVEN("a map of maps")
  {
    jk::value list;
    WHEN("from_value is called")
    {
      ossia::value v = omap{{"foo", omap{{"x", 123}}}, {"bar", omap{{"x", 4.5f}}}};
      oscr::from_ossia_value(v, list);
      THEN("the conversion is correct")
      {
        REQUIRE(
            list
            == jk::value{jmap{{"foo", jmap{{"x", 123}}}, {"bar", jmap{{"x", 4.5f}}}}});
      }

      THEN("the round-trip is correct")
      {
        ossia::value roundtrip = oscr::to_ossia_value(list);
        REQUIRE(v == roundtrip);
      }
    }
  }
}
