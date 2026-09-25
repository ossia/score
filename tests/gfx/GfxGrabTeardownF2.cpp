// A Window device torn down while its grabTo is pumping the event loop.
//
// grabTo drives frames and calls processEvents until the readback lands. An OSC
// /script message from another process reached the same instance inside that
// pump and dismantled the document: the next iteration logged
// "renderFrames: no gfx document plugin" and then dereferenced the freed
// offscreen node (SIGSEGV in grabTo). Here an offscreen Window device with
// nothing wired to it (so the grab keeps pumping) is disconnected, or deleted,
// from a zero timer that fires inside the pump. The grab must stop, say so and
// write nothing.
//
// The device's output node is registered through the GfxContext command queue
// and the device owns it too. A device destroyed before a tick drained that
// queue left the queued ADD_NODE owning the freed node, which ~GfxContext then
// freed again when the document closed. The grabs here tear the device down
// before any tick, and a third device is created and deleted outright.
//
// Registration: see test_gfx_process_grab_teardown_f2.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Gfx/WindowDevice.hpp>

#include <ossia/network/base/device.hpp>

#include <core/document/Document.hpp>

#include <QFile>
#include <QPointer>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>

#include <catch2/catch_test_macros.hpp>

namespace
{
QStringList g_warnings;
void collectWarnings(QtMsgType t, const QMessageLogContext&, const QString& msg)
{
  if(t == QtWarningMsg)
    g_warnings.push_back(msg);
}

constexpr auto kName = "GrabTeardownF2";

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

int closedWarnings()
{
  int n = 0;
  for(const auto& w : g_warnings)
    if(w.contains(QStringLiteral("was closed during the grab")))
      n++;
  return n;
}
}

TEST_CASE(
    "grabTo stops cleanly when its device goes away during the grab",
    "[gfx][window][grab][f2]")
{
  qputenv("SCORE_FORCE_OFFSCREEN_WINDOW", kName);

  bool connected = false;
  bool written = false;
  int disconnectWarnings = -1;
  int deleteWarnings = -1;
  QTemporaryDir dir;
  const QString png = dir.filePath("grab.png");

  score::test::run_in_gui_app([&](const score::GUIApplicationContext& app) {
    score::Document* doc = score::test::new_document(app);
    REQUIRE(doc != nullptr);

    auto prev = qInstallMessageHandler(collectWarnings);

    {
      auto* dev = makeDevice(doc->context());
      if(!dev)
      {
        qInstallMessageHandler(prev);
        return;
      }
      connected = true;
      g_warnings.clear();
      QTimer::singleShot(0, dev, [dev] { dev->disconnect(); });
      dev->grabTo(png);
      disconnectWarnings = closedWarnings();
      delete dev;
    }

    {
      auto* dev = makeDevice(doc->context());
      REQUIRE(dev);
      g_warnings.clear();
      QTimer::singleShot(0, dev, [dev] { delete dev; });
      dev->grabTo(png);
      deleteWarnings = closedWarnings();
    }

    delete makeDevice(doc->context());

    qInstallMessageHandler(prev);
    written = QFile::exists(png);
  });

  REQUIRE(connected);
  CHECK(disconnectWarnings == 1);
  CHECK(deleteWarnings == 1);
  CHECK_FALSE(written);
}
