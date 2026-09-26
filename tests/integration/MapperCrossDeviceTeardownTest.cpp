// A mapper script writes into another device ("Target:/x") on its own thread
// while that device is disconnected and reconnected. The script's address
// cache holds raw parameter pointers into the target's tree: they must be
// dropped before disconnect() destroys the nodes. The use-after-free this
// guards against is caught by the ASan build; elsewhere the test checks that
// the writes resume after each reconnection.
#include <score_test/Mapper.hpp>

using score::test::mapper::fixture;

namespace
{
const char* k_target = R"_(import Ossia 1.0 as Ossia
Ossia.Mapper
{
  function createTree() {
    return [ { name: "x", type: Ossia.Type.Int } ];
  }
}
)_";

const char* k_writer = R"_(import Ossia 1.0 as Ossia
Ossia.Mapper
{
  property int n: 0
  function createTree() {
    return [ {
      name: "tick",
      type: Ossia.Type.Int,
      interval: 1,
      read: function() { n++; Device.write("Target:/x", n); return n; }
    } ];
  }
}
)_";

int targetValue(fixture& f)
{
  return f.contents(QStringLiteral("Target")).value(QStringLiteral("Target:/x")).toInt();
}
}

TEST_CASE(
    "a mapper writing into another device survives that device's reconnections",
    "[mapper][teardown]")
{
  score::test::run_in_app([&](const auto& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    fixture f{ctx, *doc};
    f.createMapper(QStringLiteral("Target"), QString::fromUtf8(k_target));
    f.createMapper(QStringLiteral("Writer"), QString::fromUtf8(k_writer));

    REQUIRE(fixture::spin([&] { return targetValue(f) > 0; }));

    auto& devices = doc->context().template plugin<Explorer::DeviceDocumentPlugin>().list();
    auto* target = devices.findDevice(QStringLiteral("Target"));
    REQUIRE(target);

    for(int i = 0; i < 40; i++)
    {
      CAPTURE(i);
      target->disconnect();
      fixture::spin([] { return false; }, 5);
      REQUIRE(target->reconnect());
      const int before = targetValue(f);
      REQUIRE(fixture::spin([&] { return targetValue(f) > before; }));
    }

    f.removeMapper(QStringLiteral("Writer"));
    f.removeMapper(QStringLiteral("Target"));
  });
}
