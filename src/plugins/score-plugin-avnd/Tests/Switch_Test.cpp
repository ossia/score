#include <ossia/dataflow/execution_state.hpp>

#include <Advanced/AI/PromptComposer.hpp>
#include <AvndProcesses/Switch.hpp>
#include <avnd/binding/ossia/data_node.hpp>
#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/to_value.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
struct SwitchFixture
{
  ao::Switch object;
  ossia::execution_state state;
  ossia::value_port input, unmatched;
  std::vector<ossia::value_port> cases;

  SwitchFixture()
  {
    state.bufferSize = 4096;
    state.modelToSamplesRatio = 1.;
    state.samplesToModelRatio = 1.;
    object.ossia_state = {&state};
    object.inputs.input.value = &input;
    object.outputs.unmatched.value = &unmatched;
  }
  void edit(halp::string_list_value rows)
  {
    object.inputs.cases.value = std::move(rows);
    object.inputs.cases.update(object);
    cases.resize(object.inputs.cases.value.size());
    object.outputs.cases.ports.resize(cases.size());
    for(std::size_t i = 0; i < cases.size(); ++i)
      object.outputs.cases.ports[i].value = &cases[i];
  }
  void run(int start = 0, int frames = 256)
  {
    ao::Switch::tick t;
    t.prev_date = ossia::time_value{start};
    t.date = ossia::time_value{start + frames};
    t.start_sample = start;
    t.length_sample = frames;
    object(t);
  }
};
}

TEST_CASE(
    "Switch routes every arrival without changing payload or timestamp",
    "[avnd][switch]")
{
  SwitchFixture f;
  f.edit({{10000, "42"}, {10001, "\"42\""}, {10002, "true"}, {10003, "null"}});
  f.input.write_value(42, 3);
  f.input.write_value(42, 7);
  f.input.write_value(42.f, 9);
  f.input.write_value(std::string{"42"}, 11);
  f.input.write_value(true, 13);
  f.input.write_value(ossia::impulse{}, 15);
  f.input.write_value(false, 17);
  f.run();
  REQUIRE(f.cases[0].get_data().size() == 3);
  CHECK(f.cases[0].get_data()[0].timestamp == 3);
  CHECK(f.cases[0].get_data()[1].timestamp == 7);
  CHECK(f.cases[0].get_data()[2].value.get_type() == ossia::val_type::FLOAT);
  REQUIRE(f.cases[1].get_data().size() == 1);
  CHECK(f.cases[1].get_data()[0].value == ossia::value{std::string{"42"}});
  REQUIRE(f.cases[2].get_data().size() == 1);
  CHECK(f.cases[2].get_data()[0].value.get_type() == ossia::val_type::BOOL);
  REQUIRE(f.cases[3].get_data().size() == 1);
  CHECK(f.cases[3].get_data()[0].value.get_type() == ossia::val_type::IMPULSE);
  REQUIRE(f.unmatched.get_data().size() == 1);
  CHECK(f.unmatched.get_data()[0].value == ossia::value{false});
  CHECK(f.unmatched.get_data()[0].timestamp == 17);
}

TEST_CASE(
    "Switch duplicates choose the first row and edits change matching", "[avnd][switch]")
{
  SwitchFixture f;
  f.edit({{10000, "1"}, {10001, "1.0"}});
  f.input.write_value(1, 0);
  f.run();
  REQUIRE(f.cases[0].get_data().size() == 1);
  CHECK(f.cases[1].get_data().empty());
  f.input.clear();
  for(auto& p : f.cases)
    p.clear();
  f.edit({{10001, "2"}, {10000, "1"}});
  f.input.write_value(1, 1);
  f.run();
  CHECK(f.cases[0].get_data().empty());
  REQUIRE(f.cases[1].get_data().size() == 1);
  CHECK(f.cases[1].get_data()[0].timestamp == 1);
}

TEST_CASE("Switch empty and invalid case lists fall through", "[avnd][switch]")
{
  SwitchFixture f;
  SECTION("empty")
  {
    f.edit({});
  }
  SECTION("not JSON scalars")
  {
    f.edit({{10000, "unquoted"}, {10001, "[1]"}, {10002, "{}"}, {10003, "1 trailing"}});
  }
  const ossia::value payload{std::vector<ossia::value>{1, std::string{"unchanged"}}};
  f.input.write_value(payload, 23);
  f.run();
  for(auto& p : f.cases)
    CHECK(p.get_data().empty());
  REQUIRE(f.unmatched.get_data().size() == 1);
  CHECK(f.unmatched.get_data()[0].value == payload);
  CHECK(f.unmatched.get_data()[0].timestamp == 23);
  f.input.clear();
  f.unmatched.clear();
  f.run();
  CHECK(f.unmatched.get_data().empty());
}

TEST_CASE("Switch handles each partial-buffer event once", "[avnd][switch]")
{
  SwitchFixture f;
  f.edit({{10000, "1"}});
  f.input.write_value(1, 3);
  f.input.write_value(1, 11);
  f.input.write_value(1, 20);
  f.run(0, 8);
  REQUIRE(f.cases[0].get_data().size() == 1);
  CHECK(f.cases[0].get_data()[0].timestamp == 3);
  f.run(8, 8);
  REQUIRE(f.cases[0].get_data().size() == 2);
  CHECK(f.cases[0].get_data()[1].timestamp == 11);
}

TEST_CASE(
    "Prompt keyword migration preserves empty rows and weight identities",
    "[avnd][string-list]")
{
  ai::PromptComposer object;
  auto value
      = decltype(object.inputs.controller)::migrate_value(std::string{" sky \n\nsea\n"});
  REQUIRE(oscr::from_ossia_value(value, object.inputs.controller.value));
  const halp::string_list_value expected{
      {12000, " sky "}, {12001, ""}, {12002, "sea"}, {12003, ""}};
  CHECK(object.inputs.controller.value == expected);
  CHECK(oscr::to_ossia_value(object.inputs.controller.value) == value);
  CHECK(decltype(object.inputs.controller)::migrate_value(value) == value);
  object.inputs.in_i.ports.resize(4);
  object.inputs.in_i.ports[0].value = 0.5f;
  object.inputs.in_i.ports[1].value = 0.25f;
  object.inputs.in_i.ports[2].value = 0.75f;
  object.inputs.in_i.ports[3].value = 1.f;
  object();
  CHECK(object.outputs.out.value == "(sky:0.5), (:0.25), (sea:0.75), (:1)");
}

TEST_CASE("Switch publishes through real dynamic native output ports", "[avnd][switch]")
{
  ossia::execution_state state;
  state.bufferSize = 256;
  state.modelToSamplesRatio = 1.;
  state.samplesToModelRatio = 1.;
  oscr::safe_node<ao::Switch> host{256, 48000., 0};
  host.dynamic_ports.num_out_ports(avnd::field_index<1>{}) = 1;
  host.finish_init();
  auto& node = host.impl.effect;
  node.ossia_state = {&state};
  node.inputs.cases.value = {{10000, "\"left\""}};
  node.inputs.cases.update(node);
  auto& input = tuplet::get<0>(host.ossia_inlets.ports).data;
  input.write_value("left", 3);
  input.write_value("left", 11);
  ao::Switch::tick t;
  t.date = ossia::time_value{256};
  t.length_sample = 256;
  host.run(t, {&state});
  auto& output = tuplet::get<1>(host.ossia_outlets.ports)[0]->data.get_data();
  REQUIRE(output.size() == 2);
  CHECK(output[0].value == ossia::value{"left"});
  CHECK(output[0].timestamp == 3);
  CHECK(output[1].timestamp == 11);
}
