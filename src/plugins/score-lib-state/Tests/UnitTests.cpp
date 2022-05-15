// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "Utils.hpp"
#include <State/Unit.hpp>

#include <score/serialization/VisitorCommon.hpp>

#include <ossia/network/dataspace/dataspace.hpp>

TEST_CASE ("deserialize", "deserialize")
{
  ossia::unit_t u = ossia::rgba_u{};
  REQUIRE(u == u);

  auto v = score::unmarshall<ossia::unit_t>(score::marshall<DataStream>(u));

  REQUIRE(u == v);
}
