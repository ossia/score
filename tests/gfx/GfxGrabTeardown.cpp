// A Window device torn down while its grabTo is pumping the event loop.
//
// grabTo drives frames and calls processEvents until the readback lands, so
// anything reaching the instance inside that pump (an OSC /script message) can
// dismantle the document under it. Here an offscreen Window device with
// nothing wired to it (so the grab keeps pumping) is disconnected, or deleted,
// from a zero timer that fires inside the pump. The grab must stop, say so and
// write nothing.
//
// The device's output node is registered through the GfxContext command queue
// and the device owns it too: a device destroyed before a tick drains that
// queue must not leave the queued ADD_NODE owning the freed node. The grabs here tear the device down
// before any tick, and a third device is created and deleted outright.

#include "GfxLogCapture.hpp"

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Gfx/WindowDevice.hpp>

#include <ossia/network/base/device.hpp>

#include <core/document/Document.hpp>

#include <QFile>
#include <QPointer>
#include <QTemporaryDir>
#include <QTimer>

#include <catch2/catch_test_macros.hpp>

namespace
{
constexpr auto kName = "GrabTeardown";

Gfx::WindowDevice* makeDevice(const score::DocumentContext& ctx)
{
  Device::DeviceSettings set;
  set.protocol = Gfx::WindowProtocolFactory::static_concreteKey();
  set.name = QString::fromUtf8(kName);
  auto* dev = new Gfx::WindowDevice{set, ctx};
  if(!dev->reconnect())
  {
    delete dev;
    return nullptr;
  }
  return dev;
}

int closedWarnings(const score::test::gfx::LogCapture& log)
{
  return log.count(QtWarningMsg, {u"was closed during the grab"});
}
}

TEST_CASE(
    "grabTo stops cleanly when its device goes away during the grab",
    "[gfx][window][grab][teardown]")
{
  qputenv("SCORE_FORCE_OFFSCREEN_WINDOW", kName);

  bool connected = false;
  bool written = false;
  int disconnectWarnings = -1;
  int deleteWarnings = -1;
  QTemporaryDir dir;
  const QString png = dir.filePath("grab.png");

  // The device renders with the backend the settings select, whichever the
  // platform defaults to; what is under test is the device lifetime, not a
  // backend.
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& app) {
    score::Document* doc = score::test::new_document(app);
    if(!doc)
      return;

    score::test::gfx::LogCapture log;
    {
      auto* dev = makeDevice(doc->context());
      if(!dev)
        return;
      connected = true;
      QTimer::singleShot(0, dev, [dev] { dev->disconnect(); });
      dev->grabTo(png);
      disconnectWarnings = closedWarnings(log);
      delete dev;
    }

    {
      auto* dev = makeDevice(doc->context());
      if(!dev)
        return;
      log.clear();
      QTimer::singleShot(0, dev, [dev] { delete dev; });
      dev->grabTo(png);
      deleteWarnings = closedWarnings(log);
    }

    delete makeDevice(doc->context());
    written = QFile::exists(png);
  });
  qunsetenv("SCORE_FORCE_OFFSCREEN_WINDOW");

  REQUIRE(connected);
  CHECK(disconnectWarnings == 1);
  CHECK(deleteWarnings == 1);
  CHECK_FALSE(written);
}
