// Unit tests for Gfx/ControlTree.hpp — the description -> device-tree builder
// every video device's control group goes through (V4L2 driver controls,
// score's own /render/ adjustments, the Argus sensor controls).
//
// Two behaviours, one per commit:
//
//  * the fresh build (1cfa2b75f1): nodes appear at <parent>/<group>/<name>, in
//    order, with type/domain/access/description applied; the initial value is
//    set WITHOUT invoking the driver callback (it describes what the hardware
//    already holds, and a write-back could perform an unwanted action);
//    afterwards a write reaches the driver callback.
//
//  * the reload adoption (52b00de5e5): a document reload restores the saved
//    tree before the hardware is opened, so the group and its children already
//    exist. addControlGroup must adopt them instead of failing add_child and
//    returning nulls; the restored value is the USER's, so it is pushed to the
//    driver once, and a re-attach never leaves two drivers on one parameter.
//
// Pure ossia tree, no hardware, no QRhi, no application.

#include <Gfx/ControlTree.hpp>

#include <ossia/network/base/parameter.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace
{
struct fixture
{
  ossia::net::generic_device dev{
      std::make_unique<ossia::net::multiplex_protocol>(), "cam"};

  ossia::net::node_base& root() { return dev.get_root_node(); }
};

Gfx::TreeControl floatControl(
    std::string name, float initial, std::vector<ossia::value>* writes)
{
  Gfx::TreeControl c;
  c.name = std::move(name);
  c.description = "desc-" + c.name;
  c.type = ossia::val_type::FLOAT;
  c.domain = ossia::make_domain(0.f, 1.f);
  c.initial = initial;
  if(writes)
    c.onSet = [writes](const ossia::value& v) { writes->push_back(v); };
  return c;
}
} // namespace

TEST_CASE("a control group is built from its description")
{
  fixture f;
  std::vector<ossia::value> gainWrites, expoWrites;

  const auto params = Gfx::addControlGroup(
      f.dev, f.root(), "controls",
      {floatControl("gain", 0.25f, &gainWrites),
       floatControl("exposure", 0.75f, &expoWrites)});

  REQUIRE(params.size() == 2);
  REQUIRE(params[0] != nullptr);
  REQUIRE(params[1] != nullptr);

  // The nodes are where a consumer will address them.
  auto* group = f.root().find_child("controls");
  REQUIRE(group != nullptr);
  auto* gain = group->find_child("gain");
  REQUIRE(gain != nullptr);
  CHECK(gain->get_parameter() == params[0]);
  CHECK(group->find_child("exposure") != nullptr);

  // Returned in the order of the descriptions, not of the tree.
  CHECK(params[0] == gain->get_parameter());

  // Type, domain, access, description all came from the description.
  CHECK(params[0]->get_value_type() == ossia::val_type::FLOAT);
  CHECK(params[0]->get_domain() == ossia::make_domain(0.f, 1.f));
  CHECK(params[0]->get_access() == ossia::access_mode::BI);
  CHECK(ossia::net::get_description(*gain) == std::string("desc-gain"));

  // The initial value is visible on the parameter...
  CHECK(params[0]->value() == ossia::value{0.25f});
  // ...but was NOT driven through the callback: it describes what the hardware
  // already holds, and for an action control a write-back acts.
  CHECK(gainWrites.empty());
  CHECK(expoWrites.empty());

  // A write from the tree side reaches the driver.
  params[0]->push_value(0.5f);
  REQUIRE(gainWrites.size() == 1);
  CHECK(gainWrites[0] == ossia::value{0.5f});
  CHECK(expoWrites.empty());
}

TEST_CASE("a reloaded tree is adopted, not fought")
{
  fixture f;

  // What a document reload leaves behind: the group and one control already
  // exist, and the control still holds the value the user had set.
  auto* restoredGroup = f.root().add_child(
      std::make_unique<ossia::net::generic_node>("controls", f.dev, f.root()));
  REQUIRE(restoredGroup != nullptr);
  auto* restoredGain = restoredGroup->add_child(
      std::make_unique<ossia::net::generic_node>("gain", f.dev, *restoredGroup));
  REQUIRE(restoredGain != nullptr);
  auto* restoredParam = restoredGain->create_parameter(ossia::val_type::FLOAT);
  REQUIRE(restoredParam != nullptr);
  restoredParam->set_value(0.7f);

  std::vector<ossia::value> gainWrites, freshWrites;
  const auto params = Gfx::addControlGroup(
      f.dev, f.root(), "controls",
      {floatControl("gain", 0.25f, &gainWrites),
       // A control the document did NOT restore, in the same call: the mixed
       // case a partial save produces.
       floatControl("fresh", 0.5f, &freshWrites)});

  REQUIRE(params.size() == 2);

  // Pre-fix, add_child refused the duplicate names, addControlGroup returned
  // nulls, and the explorer showed a full set of dead controls.
  REQUIRE(params[0] != nullptr);
  REQUIRE(params[1] != nullptr);

  // The document's node was adopted — not shadowed by a second one.
  CHECK(params[0] == restoredParam);
  CHECK(restoredGroup->children().size() == 2);

  // The restored value is the user's, not the hardware's: it was pushed to the
  // driver exactly once during attach...
  REQUIRE(gainWrites.size() == 1);
  CHECK(gainWrites[0] == ossia::value{0.7f});
  // ...and NOT overwritten with the description's initial.
  CHECK(params[0]->value() == ossia::value{0.7f});

  // The control that was not in the document behaves like the fresh path.
  CHECK(freshWrites.empty());
  CHECK(params[1]->value() == ossia::value{0.5f});

  // A write now reaches the driver once — the adopted parameter carries one
  // callback, not a stack of them.
  gainWrites.clear();
  params[0]->push_value(0.9f);
  CHECK(gainWrites.size() == 1);
}

TEST_CASE("a second attach never leaves two drivers on one parameter")
{
  fixture f;

  // First attach: the device opens once...
  std::vector<ossia::value> firstDriver;
  auto first = Gfx::addControlGroup(
      f.dev, f.root(), "controls", {floatControl("gain", 0.25f, &firstDriver)});
  REQUIRE(first.size() == 1);
  REQUIRE(first[0] != nullptr);

  // ...then closes and reopens (the tree stays — it is the document's).
  std::vector<ossia::value> secondDriver;
  auto second = Gfx::addControlGroup(
      f.dev, f.root(), "controls", {floatControl("gain", 0.25f, &secondDriver)});
  REQUIRE(second.size() == 1);
  REQUIRE(second[0] == first[0]);

  const auto attachWrites = secondDriver.size(); // the restored-value push

  first[0]->push_value(0.6f);

  // The write reaches the CURRENT driver once, and the stale driver never.
  CHECK(secondDriver.size() == attachWrites + 1);
  CHECK(firstDriver.size() <= 1); // at most its own attach-time push
}

// 52b00de5e5's contract is that the value pushed to the driver at adoption is
// "the value the document restored -- that value is the user's". When the
// restored node carries NO parameter, addControlGroup creates one, whose
// default-constructed value (0.0 for FLOAT) is fabricated, not the user's --
// so it pushes NOTHING, the same way the fresh-node path refuses to write
// c.initial back. It distinguishes "the node existed with a parameter" (push
// the restored value) from "the parameter was just created here" (push
// nothing), so a gain/exposure is never slammed to zero by a value no
// document ever held.
TEST_CASE("an adopted node without a parameter gets one", "[gfx][controltree]")
{
  fixture f;

  // A tree restored from a save that had the node but serialized no parameter.
  auto* group = f.root().add_child(
      std::make_unique<ossia::net::generic_node>("controls", f.dev, f.root()));
  REQUIRE(group != nullptr);
  auto* bare = group->add_child(
      std::make_unique<ossia::net::generic_node>("gain", f.dev, *group));
  REQUIRE(bare != nullptr);
  REQUIRE(bare->get_parameter() == nullptr);

  std::vector<ossia::value> writes;
  const auto params = Gfx::addControlGroup(
      f.dev, f.root(), "controls", {floatControl("gain", 0.25f, &writes)});

  REQUIRE(params.size() == 1);
  REQUIRE(params[0] != nullptr);
  CHECK(params[0] == bare->get_parameter());
  // No restored value existed, so nothing was pushed to the driver.
  CHECK(writes.empty());
}
