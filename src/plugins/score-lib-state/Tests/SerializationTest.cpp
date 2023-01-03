// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define CATCH_CONFIG_MAIN
#include "Utils.hpp"

#include <catch2/catch_all.hpp>

using namespace score;
TEST_CASE("serializationTest", "serializationTest")
{
  State::Message m;
  m.address = {"dada", {"bilou", "yadaa", "zoo"}};
  m.value = 5.5f;

  {
    auto json = marshall<JSONObject>(m);
    auto mess_json = unmarshall<State::Message>(json);
    SCORE_ASSERT(m == mess_json);

    auto barray = marshall<DataStream>(m);
    auto mess_array = unmarshall<State::Message>(barray);
    SCORE_ASSERT(m == mess_array);
  }
}

TEST_CASE("ossia_value_serialization_test", "ossia_value_serialization_test")
{
  using namespace std::literals;
  {
    ossia::value v;
    REQUIRE(unmarshall<ossia::value>(marshall<JSONObject>(v)) == v);
    REQUIRE(unmarshall<ossia::value>(marshall<DataStream>(v)) == v);
  }
  {
    ossia::value v = ossia::impulse{};
    REQUIRE(unmarshall<ossia::value>(marshall<JSONObject>(v)) == v);
    REQUIRE(unmarshall<ossia::value>(marshall<DataStream>(v)) == v);
  }
  {
    ossia::value v(1234);
    REQUIRE(unmarshall<ossia::value>(marshall<JSONObject>(v)) == v);
    REQUIRE(unmarshall<ossia::value>(marshall<DataStream>(v)) == v);
  }
  {
    ossia::value v(1234.f);
    REQUIRE(unmarshall<ossia::value>(marshall<JSONObject>(v)) == v);
    REQUIRE(unmarshall<ossia::value>(marshall<DataStream>(v)) == v);
  }
  {
    ossia::value v('c');
    REQUIRE(unmarshall<ossia::value>(marshall<JSONObject>(v)) == v);
    REQUIRE(unmarshall<ossia::value>(marshall<DataStream>(v)) == v);
  }
  {
    ossia::value v("duh"s);
    REQUIRE(unmarshall<ossia::value>(marshall<JSONObject>(v)) == v);
    REQUIRE(unmarshall<ossia::value>(marshall<DataStream>(v)) == v);
  }
  {
    ossia::value v(ossia::vec2f{1, 2});
    REQUIRE(unmarshall<ossia::value>(marshall<JSONObject>(v)) == v);
    REQUIRE(unmarshall<ossia::value>(marshall<DataStream>(v)) == v);
  }
  {
    ossia::value v(ossia::vec3f{1, 2, 45});
    REQUIRE(unmarshall<ossia::value>(marshall<JSONObject>(v)) == v);
    REQUIRE(unmarshall<ossia::value>(marshall<DataStream>(v)) == v);
  }
  {
    ossia::value v(ossia::vec4f{1, 2, 45, 345});
    REQUIRE(unmarshall<ossia::value>(marshall<JSONObject>(v)) == v);
    REQUIRE(unmarshall<ossia::value>(marshall<DataStream>(v)) == v);
  }
  {
    ossia::value v(std::vector<ossia::value>{});
    REQUIRE(unmarshall<ossia::value>(marshall<JSONObject>(v)) == v);
    REQUIRE(unmarshall<ossia::value>(marshall<DataStream>(v)) == v);
  }
  {
    ossia::value v(std::vector<ossia::value>{1, 2, 3});
    REQUIRE(unmarshall<ossia::value>(marshall<JSONObject>(v)) == v);
    REQUIRE(unmarshall<ossia::value>(marshall<DataStream>(v)) == v);
  }
  {
    ossia::value v(std::vector<ossia::value>{
        1, "boo"s, 3, std::vector<ossia::value>{"Banana"s, "Carrot"s, 'c'}});
    REQUIRE(unmarshall<ossia::value>(marshall<JSONObject>(v)) == v);
  }
}

TEST_CASE("ossia_domain_serialization_test", "ossia_domain_serialization_test")
{
  {
    ossia::domain d;
    REQUIRE(unmarshall<ossia::domain>(marshall<JSONObject>(d)) == d);
    REQUIRE(unmarshall<ossia::domain>(marshall<DataStream>(d)) == d);
  }

  {
    ossia::domain d = ossia::make_domain(0, 1);
    REQUIRE(unmarshall<ossia::domain>(marshall<JSONObject>(d)) == d);
    REQUIRE(unmarshall<ossia::domain>(marshall<DataStream>(d)) == d);
  }

  {
    ossia::domain d = ossia::make_domain(0., 1.);
    REQUIRE(unmarshall<ossia::domain>(marshall<JSONObject>(d)) == d);
    REQUIRE(unmarshall<ossia::domain>(marshall<DataStream>(d)) == d);
  }

  {
    ossia::domain d = ossia::make_domain(false, true);
    REQUIRE(unmarshall<ossia::domain>(marshall<JSONObject>(d)) == d);
    REQUIRE(unmarshall<ossia::domain>(marshall<DataStream>(d)) == d);
  }

  {
    ossia::domain d = ossia::make_domain('a', 'z');
    REQUIRE(unmarshall<ossia::domain>(marshall<JSONObject>(d)) == d);
    REQUIRE(unmarshall<ossia::domain>(marshall<DataStream>(d)) == d);
  }

  {
    ossia::domain d = ossia::make_domain(ossia::vec2f{0, 0}, ossia::vec2f{1, 1});
    REQUIRE(unmarshall<ossia::domain>(marshall<JSONObject>(d)) == d);
    REQUIRE(unmarshall<ossia::domain>(marshall<DataStream>(d)) == d);
  }

  {
    ossia::domain d = ossia::make_domain(ossia::vec3f{0, 0, 0}, ossia::vec3f{1, 1, 1});
    REQUIRE(unmarshall<ossia::domain>(marshall<JSONObject>(d)) == d);
    REQUIRE(unmarshall<ossia::domain>(marshall<DataStream>(d)) == d);
  }

  {
    ossia::domain d
        = ossia::make_domain(ossia::vec4f{0, 0, 0, 0}, ossia::vec4f{1, 1, 1, 1});
    REQUIRE(unmarshall<ossia::domain>(marshall<JSONObject>(d)) == d);
    REQUIRE(unmarshall<ossia::domain>(marshall<DataStream>(d)) == d);
  }

  {
    ossia::domain d = ossia::make_domain(
        std::vector<ossia::value>{0, 'x'}, std::vector<ossia::value>{1, 'y'});
    REQUIRE(unmarshall<ossia::domain>(marshall<JSONObject>(d)) == d);
    REQUIRE(unmarshall<ossia::domain>(marshall<DataStream>(d)) == d);
  }

  {
    ossia::domain d = ossia::domain_base<std::string>();
    REQUIRE(unmarshall<ossia::domain>(marshall<JSONObject>(d)) == d);
    REQUIRE(unmarshall<ossia::domain>(marshall<DataStream>(d)) == d);
  }
}
