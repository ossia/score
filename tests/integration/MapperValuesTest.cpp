// The values of a Mapper device's nodes: what they start at, what they send,
// and what the explorer shows after the script is edited.
#include <score_test/Mapper.hpp>

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Explorer/Commands/Update/UpdateDeviceSettings.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <Protocols/Mapper/MapperDevice.hpp>

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/domain/domain.hpp>

#include <cmath>

using score::test::mapper::fixture;

namespace
{
Device::Node* explorerDevice(score::Document& doc, const QString& name)
{
  auto& plug = doc.context().plugin<Explorer::DeviceDocumentPlugin>();
  for(auto& n : plug.rootNode())
    if(n.get<Device::DeviceSettings>().name == name)
      return &n;
  return nullptr;
}

const Device::AddressSettings* explorerNode(score::Document& doc, const QString& dev,
                                            const QString& name)
{
  auto d = explorerDevice(doc, dev);
  if(!d)
    return nullptr;
  for(auto& n : *d)
    if(n.displayName() == name)
      return &n.get<Device::AddressSettings>();
  return nullptr;
}

ossia::net::parameter_base* parameter(score::Document& doc, const QString& dev,
                                      const char* path)
{
  auto& devices = doc.context().plugin<Explorer::DeviceDocumentPlugin>().list();
  auto* d = devices.findDevice(dev);
  if(!d || !d->getDevice())
    return nullptr;
  auto* node = ossia::net::find_node(d->getDevice()->get_root_node(), path);
  return node ? node->get_parameter() : nullptr;
}

const char* k_untyped_values = R"_(import Ossia 1.0 as Ossia
Ossia.Mapper
{
  function createTree() {
    return [
      { name: "f", type: Ossia.Type.Float },
      { name: "i", type: Ossia.Type.Int },
      { name: "b", type: Ossia.Type.Bool },
      { name: "s", type: Ossia.Type.String },
      { name: "v2", type: Ossia.Type.Vec2f },
      { name: "v3", type: Ossia.Type.Vec3f },
      { name: "v4", type: Ossia.Type.Vec4f },
      { name: "l", type: Ossia.Type.List },
      { name: "m", type: Ossia.Type.Map },
      { name: "p", type: Ossia.Type.Impulse },
      { name: "fmax", type: Ossia.Type.Float, max: 5 },
      { name: "smin", type: Ossia.Type.String, min: "a" }
    ];
  }
}
)_";
}

TEST_CASE("a mapper node given no value starts at its type's zero", "[mapper]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper(QStringLiteral("M"), QString::fromUtf8(k_untyped_values));
    REQUIRE(fixture::spin([&] { return parameter(*doc, "M", "/smin") != nullptr; }));

    struct expected
    {
      const char* path;
      ossia::value value;
    };
    const expected all[] = {
        {"/f", 0.f},
        {"/i", 0},
        {"/b", false},
        {"/s", std::string{}},
        {"/v2", ossia::vec2f{}},
        {"/v3", ossia::vec3f{}},
        {"/v4", ossia::vec4f{}},
        {"/l", std::vector<ossia::value>{}},
        {"/m", ossia::value_map_type{}},
        {"/p", ossia::impulse{}},
        {"/fmax", 0.f},
        {"/smin", std::string{}},
    };
    for(auto& e : all)
    {
      INFO(e.path);
      auto* p = parameter(*doc, "M", e.path);
      REQUIRE(p);
      CHECK(p->value() == e.value);
    }

    // A bound the script leaves out is no bound, not a nan or "undefined" one.
    auto* fmax = parameter(*doc, "M", "/fmax");
    CHECK_FALSE(ossia::get_min(fmax->get_domain()).valid());
    CHECK(ossia::get_max(fmax->get_domain()) == ossia::value{5.f});
    auto* smin = parameter(*doc, "M", "/smin");
    CHECK_FALSE(ossia::get_max(smin->get_domain()).valid());

    f.removeMapper(QStringLiteral("M"));
  });
}

namespace
{
// Every write to x is counted, and the count read back on "count".
const char* k_counted = R"_(import Ossia 1.0 as Ossia
Ossia.Mapper
{
  property int n: 0
  function createTree() {
    return [
      { name: "x", type: Ossia.Type.Int, write: function(v) { n++; } },
      { name: "count", type: Ossia.Type.Int, interval: 5,
        read: function() { return n; } }
    ];
  }
}
)_";

const char* k_follower = R"_(import Ossia 1.0 as Ossia
Ossia.Mapper
{
  function createTree() {
    return [ { name: "y", type: Ossia.Type.Int, bind: "Target:/x" } ];
  }
}
)_";
}

TEST_CASE("a mapper node follows its source without writing back to it", "[mapper]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper(QStringLiteral("Target"), QString::fromUtf8(k_counted));
    REQUIRE(fixture::spin([&] { return parameter(*doc, "Target", "/count") != nullptr; }));
    f.createMapper(QStringLiteral("Follower"), QString::fromUtf8(k_follower));
    REQUIRE(fixture::spin([&] { return parameter(*doc, "Follower", "/y") != nullptr; }));
    fixture::spin([] { return false; }, 100);

    // Opening the follower wrote nothing to its source.
    auto count = [&] { return parameter(*doc, "Target", "/count")->value(); };
    CHECK(count() == ossia::value{0});

    f.push(QStringLiteral("Target"), QStringLiteral("/x"), ossia::value{7});
    REQUIRE(fixture::spin(
        [&] { return parameter(*doc, "Follower", "/y")->value() == ossia::value{7}; }));
    REQUIRE(fixture::spin([&] { return count() == ossia::value{1}; }));
    fixture::spin([] { return false; }, 200);

    // One write, the one pushed; the follower did not echo it back.
    CHECK(count() == ossia::value{1});

    f.removeMapper(QStringLiteral("Follower"));
    f.removeMapper(QStringLiteral("Target"));
  });
}

namespace
{
const char* k_before = R"_(import Ossia 1.0 as Ossia
Ossia.Mapper
{
  function createTree() {
    return [
      { name: "a", type: Ossia.Type.Float, value: 1 },
      { name: "b", type: Ossia.Type.Int, value: 3 }
    ];
  }
}
)_";

const char* k_after = R"_(import Ossia 1.0 as Ossia
Ossia.Mapper
{
  function createTree() {
    return [
      { name: "a", type: Ossia.Type.Float, value: 2 },
      { name: "c", type: Ossia.Type.String, value: "new" }
    ];
  }
}
)_";
}

TEST_CASE("the explorer shows an edited mapper script's tree", "[mapper][explorer]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper(QStringLiteral("M"), QString::fromUtf8(k_before));
    REQUIRE(fixture::spin([&] { return explorerNode(*doc, "M", "b") != nullptr; }));
    REQUIRE(explorerNode(*doc, "M", "a")->value == ossia::value{1.f});

    // What editing the device in the explorer does for this protocol.
    auto* node = explorerDevice(*doc, "M");
    REQUIRE(node);
    auto settings = node->get<Device::DeviceSettings>();
    settings.deviceSpecificSettings = QVariant::fromValue(
        Protocols::MapperSpecificSettings{QString::fromUtf8(k_after)});
    CommandDispatcher<>{doc->context().commandStack}.submit(
        new Explorer::Command::UpdateDeviceSettings{
            doc->context().plugin<Explorer::DeviceDocumentPlugin>(), settings.name,
            settings});

    auto shows = [&](float a, const char* present, const char* gone) {
      auto* na = explorerNode(*doc, "M", "a");
      return na && na->value == ossia::value{a} && explorerNode(*doc, "M", present)
             && !explorerNode(*doc, "M", gone);
    };
    REQUIRE(fixture::spin([&] { return shows(2.f, "c", "b"); }));
    CHECK(explorerNode(*doc, "M", "c")->value == ossia::value{std::string{"new"}});

    // Undo gives the old script, and its tree, back.
    doc->commandStack().undo();
    CHECK(fixture::spin([&] { return shows(1.f, "b", "c"); }));

    f.removeMapper(QStringLiteral("M"));
  });
}
