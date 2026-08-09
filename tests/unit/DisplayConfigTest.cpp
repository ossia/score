// Configuring the displays of a machine that has no window manager.
//
// On an appliance there is no xrandr and no display panel: the platform reads
// its configuration once at startup and that is the only chance to say
// anything. So this is about producing exactly what eglfs and vkkhrdisplay
// each understand -- and, just as much, about not producing what they don't.

#include <score/gfx/DisplayConfig.hpp>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

using namespace score::gfx;

namespace
{
//! A /sys/class/drm as the kernel lays one out.
void writeConnector(
    const QString& root, const QString& card, const QString& status,
    const QString& modes)
{
  QDir{}.mkpath(root + '/' + card);
  QFile s{root + '/' + card + "/status"};
  REQUIRE(s.open(QIODevice::WriteOnly));
  s.write(status.toUtf8() + "\n");
  s.close();

  QFile m{root + '/' + card + "/modes"};
  REQUIRE(m.open(QIODevice::WriteOnly));
  m.write(modes.toUtf8());
}
}

TEST_CASE("Outputs are read from the kernel, by connector name", "[gfx]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const auto root = tmp.path();

  writeConnector(root, "card0-HDMI-A-1", "connected", "1920x1080\n1920x1080\n1280x720\n");
  writeConnector(root, "card0-HDMI-A-2", "disconnected", "");
  writeConnector(root, "card1-DP-1", "connected", "3840x2160\n");
  // Not a connector: the card itself, and whatever else lives here.
  QDir{}.mkpath(root + "/card0");
  QDir{}.mkpath(root + "/version");

  const auto outs = enumerateOutputs(root);

  REQUIRE(outs.size() == 3);

  // The card number is probe order, not identity: it is stripped.
  CHECK(outs[0].name == "HDMI-A-1");
  CHECK(outs[1].name == "HDMI-A-2");
  CHECK(outs[2].name == "DP-1");

  CHECK(outs[0].connected);
  CHECK(!outs[1].connected);
  CHECK(outs[2].connected);

  // The kernel repeats a mode per refresh rate; the same resolution twice is
  // not two choices to offer the user.
  REQUIRE(outs[0].modes.size() == 2);
  CHECK(outs[0].modes[0] == "1920x1080");
  CHECK(outs[0].modes[1] == "1280x720");

  CHECK(outs[1].modes.isEmpty());
}

TEST_CASE("A machine with no DRM at all", "[gfx]")
{
  CHECK(enumerateOutputs("/nonexistent/class/drm").isEmpty());
}

TEST_CASE("The KMS config says only what was actually set", "[gfx]")
{
  DisplaySettings s;
  s.outputs.push_back(DisplayOutputSettings{.name = "HDMI-A-1", .mode = "1920x1080"});

  const auto doc = QJsonDocument::fromJson(toKmsConfig(s));
  REQUIRE(doc.isObject());
  const auto root = doc.object();

  // An absent key means "whatever the driver decided", which beats anything
  // score could invent: writing defaults would silently override the display.
  CHECK(!root.contains("device"));
  CHECK(!root.contains("hwcursor"));
  CHECK(!root.contains("headless"));
  CHECK(!root.contains("virtualDesktopLayout"));

  REQUIRE(root["outputs"].toArray().size() == 1);
  const auto o = root["outputs"].toArray()[0].toObject();
  CHECK(o["name"].toString() == "HDMI-A-1");
  CHECK(o["mode"].toString() == "1920x1080");
  CHECK(!o.contains("primary"));
  CHECK(!o.contains("virtualPos"));
  CHECK(!o.contains("clones"));
}

TEST_CASE("Two outputs, placed and formatted", "[gfx]")
{
  DisplaySettings s;
  s.verticalLayout = true;
  s.hardwareCursor = false;
  s.outputs.push_back(DisplayOutputSettings{
      .name = "HDMI-A-1",
      .mode = "1920x1080@60",
      .format = "argb8888",
      .primary = true,
      .x = 0,
      .y = 0,
      .hasPosition = true});
  s.outputs.push_back(DisplayOutputSettings{
      .name = "HDMI-A-2", .mode = "off", .x = 0, .y = 1080, .hasPosition = true});

  const auto root = QJsonDocument::fromJson(toKmsConfig(s)).object();

  CHECK(root["virtualDesktopLayout"].toString() == "vertical");
  CHECK(root["hwcursor"].toBool() == false);

  const auto arr = root["outputs"].toArray();
  REQUIRE(arr.size() == 2);

  CHECK(arr[0].toObject()["primary"].toBool());
  CHECK(arr[0].toObject()["format"].toString() == "argb8888");
  // The platform parses this itself, so the spelling matters.
  CHECK(arr[1].toObject()["virtualPos"].toString() == "0, 1080");
  CHECK(arr[1].toObject()["mode"].toString() == "off");
}

TEST_CASE("Settings survive a round trip through the file", "[gfx]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const auto path = tmp.path() + "/sub/display.json";

  DisplaySettings s;
  s.headless = "1920x1080";
  s.rotation = 90;
  s.hideCursor = true;
  s.vulkanDisplayIndex = 1;
  s.vulkanModeIndex = 3;
  s.outputs.push_back(DisplayOutputSettings{
      .name = "DP-1",
      .mode = "3840x2160",
      .primary = true,
      .x = 100,
      .y = 200,
      .hasPosition = true,
      .physicalWidthMm = 600,
      .cloneOf = "HDMI-A-1"});

  REQUIRE(saveDisplaySettings(s, path));

  const auto back = loadDisplaySettings(path);
  CHECK(back.headless == "1920x1080");
  CHECK(back.rotation == 90);
  CHECK(back.hideCursor);
  CHECK(back.vulkanDisplayIndex == 1);
  CHECK(back.vulkanModeIndex == 3);
  // Untouched stays untouched rather than becoming 0, which would mean
  // "physical device 0" to the platform.
  CHECK(back.vulkanPhysicalDeviceIndex == -1);

  REQUIRE(back.outputs.size() == 1);
  const auto& o = back.outputs[0];
  CHECK(o.name == "DP-1");
  CHECK(o.mode == "3840x2160");
  CHECK(o.primary);
  CHECK(o.hasPosition);
  CHECK(o.x == 100);
  CHECK(o.y == 200);
  CHECK(o.physicalWidthMm == 600);
  CHECK(o.cloneOf == "HDMI-A-1");
}

TEST_CASE("A missing or broken file is not a configuration", "[gfx]")
{
  CHECK(loadDisplaySettings("/nonexistent/display.json").isEmpty());

  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const auto path = tmp.path() + "/display.json";
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write("this is not json");
  f.close();

  CHECK(loadDisplaySettings(path).isEmpty());
}

TEST_CASE("What each platform can actually be told", "[gfx]")
{
  // eglfs takes connector names and modes through its JSON.
  CHECK(displayCapabilities("eglfs").perOutputConfiguration);
  CHECK(!displayCapabilities("eglfs").indexedDisplaySelection);

  // vkkhrdisplay has three integers and nothing else -- no names, no layout,
  // no cloning. The settings UI has to offer less rather than pretend.
  CHECK(displayCapabilities("vkkhrdisplay").indexedDisplaySelection);
  CHECK(!displayCapabilities("vkkhrdisplay").perOutputConfiguration);

  // Where a window manager owns the display, none of this applies.
  for(const auto* p : {"xcb", "wayland", "cocoa", "windows", "offscreen"})
    CHECK(!displayCapabilities(p).anyConfiguration());
}
