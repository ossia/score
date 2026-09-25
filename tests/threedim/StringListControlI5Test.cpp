// The list inputs of Create Collection, Configure Primitive and Scene Graph
// Filter (N91) are string-list controls. The control stores rows as
// [[key, text], ...] (Process::StringListEditor's value); a cable or a script
// may send a plain list of strings or a single string. The node must read the
// texts in every case, through the same oscr::from_ossia_value call the CPU
// and GPU executors make. Without the Threedim overload, the generic vector
// conversion turns each [key, text] row into one garbage string.

#include <Threedim/ConfigurePrimitive.hpp>
#include <Threedim/CreateCollection.hpp>
#include <Threedim/SceneGraphFilter.hpp>
#include <Threedim/StringListControl.hpp>

#include <avnd/binding/ossia/from_value.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace
{
using strings = std::vector<std::string>;

ossia::value keyed(std::vector<std::pair<int, std::string>> rows)
{
  std::vector<ossia::value> v;
  for(auto& [k, t] : rows)
    v.emplace_back(std::vector<ossia::value>{k, t});
  return v;
}

ossia::value flat(strings rows)
{
  std::vector<ossia::value> v;
  for(auto& t : rows)
    v.emplace_back(t);
  return v;
}

template <typename Field>
strings read(Field& field, const ossia::value& v)
{
  oscr::from_ossia_value(field, v, field.value);
  return strings(field.value.begin(), field.value.end());
}
}

TEST_CASE("A string-list control reads the texts of keyed rows", "[threedim][n91]")
{
  Threedim::SceneGraphFilter n;
  CHECK(read(n.inputs.names, keyed({{10000, "Car"}, {10001, "Wheel"}}))
        == strings{"Car", "Wheel"});
  CHECK(read(n.inputs.paths, keyed({{10003, "/Car/**"}})) == strings{"/Car/**"});
  CHECK(read(n.inputs.material_tags, keyed({})) == strings{});

  Threedim::CreateCollection c;
  CHECK(read(c.inputs.paths, keyed({{10000, "/a"}, {10001, "/b"}}))
        == strings{"/a", "/b"});
  CHECK(read(c.inputs.tags, keyed({{10000, "t"}})) == strings{"t"});

  Threedim::ConfigurePrimitive p;
  CHECK(read(p.inputs.paths, keyed({{10000, "*/chairs/*"}})) == strings{"*/chairs/*"});
}

TEST_CASE("A string-list control reads plain lists and single strings", "[threedim][n91]")
{
  Threedim::SceneGraphFilter n;
  CHECK(read(n.inputs.names, flat({"Door", "Hood"})) == strings{"Door", "Hood"});
  CHECK(read(n.inputs.names, ossia::value{std::string{"Door"}}) == strings{"Door"});

  n.inputs.names.value = {"stale"};
  CHECK(read(n.inputs.names, flat({})) == strings{});
}

TEST_CASE("A string-list value is keyed for the editor", "[threedim][n91]")
{
  using Threedim::keyed_string_list;
  CHECK(keyed_string_list(flat({"a", "b"})) == keyed({{10000, "a"}, {10001, "b"}}));
  CHECK(keyed_string_list(ossia::value{std::string{"a"}}) == keyed({{10000, "a"}}));

  const auto already = keyed({{10004, "x"}, {10002, "y"}});
  CHECK(keyed_string_list(already) == already);

  std::vector<ossia::value> mixed{
      std::vector<ossia::value>{10004, std::string{"x"}}, std::string{"new"}, 3.f};
  CHECK(keyed_string_list(mixed) == keyed({{10004, "x"}, {10005, "new"}}));
}
