// The protocol factories of score-plugin-gfx, driven through the JS scripting
// API in a real ossia-score process.
//
// Score.createDevice(name, uuid, obj) converts obj to JSON and hands it to the
// protocol's makeProtocolSpecificSettings, i.e. straight to that protocol's
// JSONWriter::write. Every key in that object is therefore optional and every
// value is whatever the script put there -- and the same is true of a .score
// file, which is JSON a user can edit.
//
// rapidjson's operator[] and GetString()/GetInt()/GetDouble() do not fail on an
// absent or mistyped member: they RAPIDJSON_ASSERT, which is a live assert() in
// a build without NDEBUG. A settings writer that reaches for obj["Key"] without
// checking therefore takes the whole application down. Before the guards, every
// gfx protocol here except Window did exactly that on `{}` -- SIGABRT, 134,
// no document, no message a user could act on.
//
// Nothing here needs a display: no device is expected to connect. What is
// asserted is that the factory is registered, that its settings parser accepts
// or rejects rather than aborts, and that a well-formed settings object
// survives the trip through the saved document.

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace
{
QString appBinary()
{
#if defined(SCORE_APP_BINARY)
  return QStringLiteral(SCORE_APP_BINARY);
#else
  return {};
#endif
}

struct Protocol
{
  const char* name;
  const char* uuid;
  const char* prettyName;
};

// The factories score-plugin-gfx registers on every platform it is built for.
constexpr Protocol protocols[] = {
    {"Camera", "d615690b-f2e2-447b-b70e-a800552db69c", "Camera input"},
    {"Window", "5a181207-7d40-4ad8-814e-879fcdf8cc31", "Window"},
    {"PipewireInput", "cf6a355f-34d1-4d24-a6ea-3d204f93cde9", "PipeWire Video Input"},
    {"PipewireOutput", "d5e7b22b-b7f6-4680-9610-2457509b7946", "PipeWire Video Output"},
    {"GStreamer", "2c644357-16a4-4c25-9e27-8e5c4a9a647d", "GStreamer"},
    {"GPhoto2", "a7e5e6cc-3e7e-4f92-b5f6-0dca37e64c8a", "GPhoto2 DSLR"},
    {"ShmdataInput", "8062b2e5-c589-41f1-8977-96c5ba782f95", "Shmdata Input"},
    {"ShmdataOutput", "69bb8215-dae2-4ec9-b60c-79f4f4fc2390", "Shmdata Output"},
    {"Sh4ltInput", "7b3a7adb-af9e-4dd5-9bd7-641f4d33fa2d", "Sh4lt Input"},
    {"Sh4ltOutput", "41e367e1-fc36-40b2-b8c4-8aecd5dfd4fc", "Sh4lt Output"},
    {"WindowCapture", "a7c1e3f0-5d2b-4e8a-9f6c-1b3d5e7a9c0f", "Window Capture"},
    {"Libav", "8b3e4f2a-1d5c-4e7b-a9f3-6c2d8e4b1a7f", "FFmpeg"},
};

// Registered only where their SDK exists. Asserted present on their platform,
// asserted ABSENT elsewhere: a factory that appeared on Linux would mean the
// build wired a stub in.
constexpr Protocol spout[] = {
    {"SpoutInput", "3c995cb6-052b-4c52-a8fd-841b33b81b29", "Spout Input"},
    {"SpoutOutput", "ddf45db7-9eaf-453c-8fc0-86ccdf21677c", "Spout Output"},
};
constexpr Protocol syphon[] = {
    {"SyphonInput", "398CEC01-C4EA-43B7-8281-D848748E0F68", "Syphon Input"},
    {"SyphonOutput", "087D032D-9A42-4BC9-B3DF-AD9BA9E86C07", "Syphon Output"},
};

struct Run
{
  QString log;
  int exitCode{-1};
  bool crashed{false};
};

Run runScript(const QString& js)
{
  auto env = QProcessEnvironment::systemEnvironment();
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  env.insert("SCORE_SANITIZE_SKIP_CHECKS", "1");
  env.insert("QT_FORCE_STDERR_LOGGING", "1");
  env.insert("QT_ASSUME_STDERR_HAS_CONSOLE", "1");

  QProcess p;
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start(
      appBinary(), {"--no-gui", "--no-restore", "--script", js, "--wait", "0"});

  Run r;
  if(!p.waitForStarted(30000) || !p.waitForFinished(300000))
  {
    p.kill();
    p.waitForFinished(5000);
    r.log = QString::fromUtf8(p.readAll());
    return r;
  }
  r.log = QString::fromUtf8(p.readAll());
  r.crashed = p.exitStatus() != QProcess::NormalExit;
  r.exitCode = p.exitCode();
  return r;
}

QString writeScript(const QTemporaryDir& dir, const QString& name, const QString& src)
{
  const QString path = dir.filePath(name);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(src.toUtf8());
  f.close();
  return path;
}

// The last protocol whose TRY was printed with no matching OK: on an abort that
// is the one whose settings parser died, and the whole point of printing both.
QString lastUnfinished(const QString& log)
{
  QString pending;
  for(const QString& line : log.split('\n'))
  {
    if(const auto i = line.indexOf("TRY "); i >= 0)
      pending = line.mid(i + 4).trimmed();
    else if(const auto j = line.indexOf("OK "); j >= 0)
      if(line.mid(j + 3).trimmed() == pending)
        pending.clear();
  }
  return pending;
}

void requireBinary()
{
  REQUIRE_FALSE(appBinary().isEmpty());
  REQUIRE(QFile::exists(appBinary()));
}

// Every device creation bracketed by a marker pair, all in one process, so a
// single run names the protocol that failed instead of only proving that one
// did.
QString creationScript(const QString& settingsExpr)
{
  QString src = QStringLiteral("var uuids = [\n");
  for(const auto& p : protocols)
    src += QStringLiteral("  [\"%1\", \"%2\"],\n").arg(p.name, p.uuid);
  src += QStringLiteral("];\n");
  src += QStringLiteral(R"JS(
for(var i = 0; i < uuids.length; i++) {
    var name = uuids[i][0];
    console.log("TRY " + name);
    Score.createDevice("Dev" + i, uuids[i][1], %1);
    console.log("OK " + name);
}
console.log("ALL-CREATED");
Qt.exit(0);
)JS")
             .arg(settingsExpr);
  return src;
}

// Recursive search for a JSON object that carries every one of `keys`.
QJsonObject findObjectWith(const QJsonValue& v, const QStringList& keys)
{
  if(v.isObject())
  {
    const auto o = v.toObject();
    bool all = true;
    for(const auto& k : keys)
      all = all && o.contains(k);
    if(all)
      return o;
    for(const auto& k : o.keys())
      if(const auto found = findObjectWith(o.value(k), keys); !found.isEmpty())
        return found;
  }
  else if(v.isArray())
  {
    for(const auto& e : v.toArray())
      if(const auto found = findObjectWith(e, keys); !found.isEmpty())
        return found;
  }
  return {};
}
}

TEST_CASE("score-plugin-gfx registers its protocol factories", "[integration][gfx][device]")
{
  requireBinary();
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  QString src = QStringLiteral("var P = Score.availableProtocols();\n");
  const auto emitCheck = [&src](const Protocol& p) {
    src += QStringLiteral(
               "console.log(\"PROTO %1 \" + (P[\"%2\"] ? \"PRESENT \" + "
               "P[\"%2\"].Name : \"ABSENT\"));\n")
               .arg(p.name, p.uuid);
  };
  for(const auto& p : protocols)
    emitCheck(p);
  for(const auto& p : spout)
    emitCheck(p);
  for(const auto& p : syphon)
    emitCheck(p);
  src += QStringLiteral("Qt.exit(0);\n");

  const auto r = runScript(writeScript(dir, "protocols.js", src));
  INFO(r.log.toStdString());
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);

  for(const auto& p : protocols)
  {
    INFO("protocol " << p.name);
    CHECK(r.log.contains(QStringLiteral("PROTO %1 PRESENT %2")
                             .arg(p.name, p.prettyName)));
  }

  SECTION("Spout is Windows-only and Syphon is macOS-only")
  {
    for(const auto& p : spout)
    {
      INFO("protocol " << p.name);
#if defined(_WIN32)
      CHECK(r.log.contains(QStringLiteral("PROTO %1 PRESENT").arg(p.name)));
#else
      CHECK(r.log.contains(QStringLiteral("PROTO %1 ABSENT").arg(p.name)));
#endif
    }
    for(const auto& p : syphon)
    {
      INFO("protocol " << p.name);
#if defined(__APPLE__)
      CHECK(r.log.contains(QStringLiteral("PROTO %1 PRESENT").arg(p.name)));
#else
      CHECK(r.log.contains(QStringLiteral("PROTO %1 ABSENT").arg(p.name)));
#endif
    }
  }
}

TEST_CASE(
    "gfx device settings parsers do not abort on hostile JSON",
    "[integration][gfx][device]")
{
  requireBinary();
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  SECTION("an empty settings object")
  {
    const auto r
        = runScript(writeScript(dir, "empty.js", creationScript(QStringLiteral("{}"))));
    INFO(r.log.toStdString());
    INFO("died on protocol: " << lastUnfinished(r.log).toStdString());
    CHECK(r.log.contains("ALL-CREATED"));
    CHECK_FALSE(r.log.contains("rapidjson"));
  }

  SECTION("every value of the wrong type")
  {
    // A string where a number is expected, a number where a string is, an
    // object where an array is: the three shapes GetInt/GetString/GetArray
    // assert on. Keys are the union of every gfx protocol's; an unknown key is
    // ignored by design, so one object can drive them all.
    const QString hostile = QStringLiteral(
        R"({ "Input": 1, "Device": [], "Size": "big", "FPS": "fast",
             "Codec": "mjpeg", "PixelFormat": {}, "ColorRange": "full",
             "Custom": "yes", "Path": 3, "Width": "wide", "Height": null,
             "Rate": [], "Mode": "single", "Outputs": 7, "InputWidth": "x",
             "InputHeight": {}, "SwapchainFlag": "f", "SwapchainFormat": [],
             "Pipeline": 0, "AudioChannels": "two", "InputTransfer": "dma",
             "Model": 1, "Port": 2, "Direction": "in", "Threads": "many",
             "AudioEncoderShort": 1, "AudioEncoderLong": 1, "AudioSmpFmt": 1,
             "AudioSampleRate": "48k", "VideoEncoderShort": 1,
             "VideoEncoderLong": 1, "VideoRenderPixFmt": 1,
             "VideoConvertedPixFmt": 1, "Muxer": 1, "MuxerLong": 1,
             "Options": "none", "WindowTitle": 1, "WindowId": "w",
             "ScreenId": "s", "ScreenName": 1, "RegionX": "0", "RegionY": "0",
             "RegionW": "0", "RegionH": "0", "RGB": 1, "IR": 1, "Depth": 1,
             "Background": 1 })");
    const auto r
        = runScript(writeScript(dir, "mistyped.js", creationScript(hostile)));
    INFO(r.log.toStdString());
    INFO("died on protocol: " << lastUnfinished(r.log).toStdString());
    CHECK(r.log.contains("ALL-CREATED"));
    CHECK_FALSE(r.log.contains("rapidjson"));
  }
}

// The parse is what the guards fixed; the exit is a separate set of defects the
// guards uncovered, because before them the process aborted on the first device
// and never reached teardown at all.
//
// Creating each of the twelve alone and exiting isolates three, each
// attributable to one protocol; the other nine exit 0:
//
//  * PipewireInput -- heap-use-after-free at Gfx/GfxInputDevice.cpp:101 in
//    Gfx::video_texture_input_parameter::~video_texture_input_parameter. The
//    GfxContext it calls unregister_node() on was freed by
//    Gfx::DocumentPlugin::~DocumentPlugin (Gfx/GfxApplicationPlugin.cpp:19),
//    which runs with the document model while the device list outlives it.
//  * GPhoto2 -- the same shape in
//    Gfx::GPhoto2::gphoto2_parameter::~gphoto2_parameter.
//  * ShmdataOutput -- no ASan report; the writer degrades correctly on an empty
//    Path, but the output still builds a render target it never frees, and Qt's
//    VulkanMemoryAllocator aborts on "Some allocations were not freed before
//    destruction of this memory block". Rate 0 also reaches Timers.cpp:95 and
//    :179 as inf, which UBSan flags and which lands as
//    "QObject::startTimer: negative intervals aren't allowed".
//
// This case was pinned [!shouldfail] with the CORRECT expectation -- a
// script that creates a gfx device and exits must exit 0 -- while the
// three defects above made the exit dirty. The observable symptom (a
// non-zero exit / crash on teardown) is gone since plug-in teardown was
// made reverse-order (#2245): the device list no longer outlives the
// GfxContext its parameters unregister from. The tag is off; the same
// expectation now runs green. The ShmdataOutput render-target leak note
// above may still hold under a leak checker -- it no longer changes the
// exit code, which is what this asserts.
TEST_CASE(
    "a script that creates gfx input devices exits cleanly",
    "[integration][gfx][device]")
{
  requireBinary();
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const auto r
      = runScript(writeScript(dir, "exit.js", creationScript(QStringLiteral("{}"))));
  INFO(r.log.toStdString());
  REQUIRE(r.log.contains("ALL-CREATED"));
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);
}

TEST_CASE(
    "a well-formed camera settings object survives the document round-trip",
    "[integration][gfx][device]")
{
  requireBinary();
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  const QString saved = dir.filePath("cam.score");
  const QString src
      = QStringLiteral(R"JS(
Score.createDevice("Cam", "d615690b-f2e2-447b-b70e-a800552db69c", {
    "Input": "/dev/video9", "Device": "test-cam",
    "Size": [1280, 720], "FPS": 30,
    "Codec": 7, "PixelFormat": 2, "ColorRange": 1, "Custom": true
});
Score.saveAs("%1");
console.log("SAVED");
Qt.exit(0);
)JS")
            .arg(saved);

  const auto r = runScript(writeScript(dir, "camera.js", src));
  INFO(r.log.toStdString());
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);
  REQUIRE(r.log.contains("SAVED"));

  QFile f{saved};
  REQUIRE(f.open(QIODevice::ReadOnly));
  const auto doc = QJsonDocument::fromJson(f.readAll());
  REQUIRE_FALSE(doc.isNull());

  const auto settings
      = findObjectWith(doc.object(), {"Input", "Device", "Size", "FPS", "Codec"});
  REQUIRE_FALSE(settings.isEmpty());
  CHECK(settings.value("Input").toString() == "/dev/video9");
  CHECK(settings.value("Device").toString() == "test-cam");
  CHECK(settings.value("FPS").toDouble() == 30.);
  CHECK(settings.value("Codec").toInt() == 7);
  CHECK(settings.value("PixelFormat").toInt() == 2);
  CHECK(settings.value("ColorRange").toInt() == 1);
  CHECK(settings.value("Custom").toBool() == true);
  const auto size = settings.value("Size").toArray();
  REQUIRE(size.size() == 2);
  CHECK(size[0].toInt() == 1280);
  CHECK(size[1].toInt() == 720);
}
