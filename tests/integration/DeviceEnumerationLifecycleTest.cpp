// The input-backend contract a QML interface relies on: enumerate a
// video-input protocol, hand an enumerated DeviceIdentifier's settings
// straight back to Score.createDevice, add a device while the transport runs,
// and switch backend between plays inside one undo step.
//
// A script reaches this API the way the console panel does -- a QQmlEngine with
// JS::EditJsContext bound to `Score` -- so that is what these cases drive, on a
// real document with real devices, rather than the C++ command layer below it.
// The difference matters: the JS conversion of `dev.settings` on the way into
// createDevice, and the QML garbage collector's claim on what
// Score.enumerateDevices() returns, exist only on the script path.
//
// Everything is read back through exported symbols and the Qt property system
// rather than by casting to JS::DeviceIdentifier / JS::GlobalDeviceEnumerator:
// those two classes' metaobjects are hidden inside the plug-in, and going
// through `property("settings")` is also literally what QML does.
//
// Where each case comes from:
//
//  1. The round-trip is the shared contract: for a sync-enumerating protocol
//     such as Camera, a script reads enumerator.devices once and reuses
//     `cameraList[i].settings` straight back into createDevice. Capture
//     backends like Syphon are used the same way.
//
//  2. Both objects Score.enumerateDevices() hands a script have to outlive
//     the script's use of them: the enumerator itself (a QObject returned
//     from a slot with no parent would be the collector's, and a script may
//     well drop it into a function-local `let`) and the DeviceIdentifiers it
//     exposes, which a script stores and reads `.settings` off after a
//     re-enumeration.
//
//  3. Adding a device while the transport runs is the ONE device-tree edit the
//     domain contract allows. A UI reaches it from a handler that carries
//     no isRunning guard: the removeDevice in it is refused during playback,
//     and the createDevice that follows therefore lands mid-playback.
//
//  3b. Which means it lands under a name that is still taken -- the remove
//     having been refused. A second device of the same name shadows the
//     first instead of replacing it, so createDevice refuses the name
//     (loudly) while still allowing the add the contract permits.
//
//  4. Backend switching BETWEEN plays: removeDevice/createDevice/setAddress
//     wrapped in startMacro/endMacro, which has to be ONE undo step whose undo
//     puts the previous device back byte-for-byte.
//
// Deliberately NOT covered as a supported path, and why: a UI that performs
// removeDevice + createDevice + setAddress while isRunning is removing a
// device during execution, which is out of contract and therefore a caller-side
// violation; cases 3 and 3b cover only the half of it the contract permits.

#include <State/Address.hpp>

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <JS/Qml/EditContext.hpp>
#include <Transport/TransportInterface.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/serialization/JSONVisitor.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QElapsedTimer>
#include <QFile>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QPointer>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QUdpSocket>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <optional>

namespace
{
// The four video-input backend UUIDs a capture UI offers.
constexpr auto kCameraUuid = "d615690b-f2e2-447b-b70e-a800552db69c";
constexpr auto kNdiUuid = "ae78b7c6-6400-483e-b45b-fd6ff87ec700";
constexpr auto kSpoutUuid = "3c995cb6-052b-4c52-a8fd-841b33b81b29";
constexpr auto kSyphonUuid = "398cec01-c4ea-43b7-8281-d848748e0f68";

//! ISF passthrough: one image inlet, one image outlet. The inlet is what the
//! apps address to "<backend>:/".
//!
//! Gfx::Filter::Model only parses its construction data when it looks like a
//! shader PATH; handed source text it builds a process with no inlets at all,
//! so this has to go through a file.
constexpr auto kIsfUuid = "74ca45ff-92c9-44a0-8f1a-754dea05ee1b";
constexpr auto kIsfPassthrough = R"_(/*{
  "DESCRIPTION": "passthrough",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST"],
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image" } ]
}*/
void main() { gl_FragColor = IMG_NORM_PIXEL(inputImage, isf_FragNormCoord); }
)_";

/// The scripting environment of the console panel, which is also the one the
/// three apps run in: JS::ApplicationPlugin binds this very object to `Score`.
struct Console
{
  QQmlEngine engine;
  JS::EditJsContext* api{};

  Console()
      : api{new JS::EditJsContext}
  {
    engine.globalObject().setProperty("Score", engine.newQObject(api));
  }

  QJSValue eval(const QString& js)
  {
    auto res = engine.evaluate(js);
    if(res.isError())
      FAIL(
          "script failed: " << res.toString().toStdString()
                            << "\nscript was: " << js.toStdString());
    return res;
  }
};

//! An ISF filter on the root interval, and its image inlet: the port the apps
//! address to the input device.
Process::Inlet* isfImageInlet(
    Console& c, Scenario::IntervalModel& interval, const QTemporaryDir& dir)
{
  const QString path = dir.filePath("passthrough.fs");
  {
    QFile f{path};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(kIsfPassthrough);
  }
  auto* proc = qobject_cast<Process::ProcessModel*>(
      c.api->createProcess(&interval, QString{kIsfUuid}, path));
  REQUIRE(proc != nullptr);
  REQUIRE(!proc->inlets().empty());
  return proc->inlets().front();
}

bool protocolPresent(const score::GUIApplicationContext& ctx, const char* uuid)
{
  const auto key = UuidKey<Device::ProtocolFactory>::fromString(QString{uuid});
  return ctx.interfaces<Device::ProtocolFactoryList>().get(key) != nullptr;
}

Explorer::DeviceDocumentPlugin& explorerPlugin(score::Document& doc)
{
  return doc.context().plugin<Explorer::DeviceDocumentPlugin>();
}

//! The device as the tree holds it. Both halves are required: the
//! DeviceInterface is what carries the settings and does the I/O, the explorer
//! node is what makes "Name:/" resolvable from a port at all.
Device::DeviceInterface* treeDevice(score::Document& doc, const QString& name)
{
  auto& plug = explorerPlugin(doc);
  auto* dev = plug.list().findDevice(name);
  auto* node = Device::try_getNodeFromString(plug.rootNode(), QStringList{name});
  if(!dev || !node)
    return nullptr;
  return dev;
}

//! The protocol-specific settings in the form score itself persists them --
//! CameraProtocolFactory::serializeProtocolSpecificSettings, reached through
//! JSONReader::read(const Device::DeviceSettings&). Used as the round-trip
//! oracle because it is protocol-agnostic (the same assertion works for NDI,
//! Spout and Syphon) and because the alternative, comparing the QVariants,
//! silently always succeeds: none of the settings structs declares operator==,
//! so QVariant comparison of two of them is meaningless.
//!
//! The device name is normalised out: createDevice takes the name from its own
//! first argument, so it legitimately differs from the enumerated one.
QJsonObject settingsJson(const Device::DeviceSettings& s)
{
  Device::DeviceSettings copy = s;
  copy.name = QStringLiteral("_");
  auto r = JSONReader::marshall(copy);
  const QByteArray bytes{r.buffer.GetString(), (int)r.buffer.GetSize()};
  return QJsonDocument::fromJson(bytes).object();
}

std::string describe(const QJsonObject& o)
{
  return QJsonDocument{o}.toJson(QJsonDocument::Compact).toStdString();
}

void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
}

template <typename F>
bool eventually(F&& f, int timeoutMs = 5000)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < timeoutMs)
  {
    if(f())
      return true;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
  }
  return f();
}

/// Stops the transport however the test leaves the stack: a document torn down
/// while playing takes its execution components down in the wrong order.
struct StopOnExit
{
  Transport::TransportInterface& transport;
  ~StopOnExit()
  {
    transport.requestStop();
    spin(200);
  }
};

/// The scripting API reports a refusal it cannot turn into an exception the
/// way the rest of the device layer does: a warning. "Refused" therefore means
/// both "nothing happened to the document" and "the user was told".
struct WarningCatcher
{
  static inline std::vector<QString> messages;
  QtMessageHandler previous{};

  WarningCatcher()
  {
    messages.clear();
    previous = qInstallMessageHandler(
        [](QtMsgType t, const QMessageLogContext&, const QString& msg) {
      if(t == QtWarningMsg || t == QtCriticalMsg || t == QtFatalMsg)
        messages.push_back(msg);
    });
  }
  ~WarningCatcher() { qInstallMessageHandler(previous); }

  bool said(const QString& what) const
  {
    for(const auto& m : messages)
      if(m.contains(what))
        return true;
    return false;
  }

  static std::string joined()
  {
    QStringList l;
    for(const auto& m : messages)
      l += m;
    return l.join(" | ").toStdString();
  }
};

//! One enumerated identifier, read the way the apps read them: through the
//! properties of the DeviceIdentifier QML sees.
struct Enumerated
{
  QString category;
  QString name;
  Device::DeviceSettings settings;
};

//! The identifiers an enumerator is currently exposing, read through the Qt
//! property system -- which is exactly what QML does for `dev.category`,
//! `dev.name` and `dev.settings`.
std::vector<Enumerated> readDevices(Console& c, const QString& jsVar)
{
  const int n = c.eval(jsVar + ".devices.length").toInt();

  std::vector<Enumerated> out;
  out.reserve(n);
  for(int i = 0; i < n; i++)
  {
    auto* ident = c.eval(QString{"%1.devices[%2]"}.arg(jsVar).arg(i)).toQObject();
    if(!ident)
      FAIL(jsVar.toStdString() << ".devices[" << i << "] is null");
    out.push_back(
        {ident->property("category").toString(), ident->property("name").toString(),
         ident->property("settings").value<Device::DeviceSettings>()});
  }
  return out;
}

//! `var e = Score.enumerateDevices(uuid); e.enumerate = true; <read e.devices>`
//! -- the enumeration idiom verbatim. Leaves `e` in the JS global scope so the
//! caller can pass `e.devices[i].settings` straight back to createDevice, which
//! is the point of the contract.
std::vector<Enumerated> enumerateVia(Console& c, const char* uuid)
{
  c.eval(
      QString{"var e = Score.enumerateDevices(\"%1\"); e.enumerate = true;"}.arg(uuid));
  return readDevices(c, "e");
}

//! The whole enumerated list as one comparable, printable blob: an assertion on
//! this says "same sources, same order, same settings" in one line and prints
//! the difference when it does not hold.
std::string signature(const std::vector<Enumerated>& v)
{
  QStringList lines;
  for(const auto& e : v)
    lines += QString{"%1 | %2 | %3"}.arg(
        e.category, e.name, QString::fromStdString(describe(settingsJson(e.settings))));
  return lines.join('\n').toStdString();
}
}

// 1. The enumerate-then-create round-trip, for Camera.
//
// The contract is the identity of the middle step: whatever the enumerator put
// in DeviceIdentifier.settings.deviceSpecificSettings has to arrive at the
// created device unchanged. createDevice reaches that only through its
// `canConvert<Device::DeviceSettings>()` branch; the QVariantMap branch below
// it re-serialises through JSON, and falling into it -- or into neither, which
// is what happens when the QJSValue overload cannot convert -- leaves the
// device with EMPTY protocol settings. That is what the user sees as "I picked
// the 1920x1080 MJPEG mode and got a black preview".
//
// The device must also be usable, not merely constructed: addressable as
// "<name>:/" from a process inlet, which is the single thing the apps do with
// it, and -- when a real capture path was enumerated -- actually opening and
// publishing the texture parameter the gfx graph pulls from.
TEST_CASE(
    "an enumerated Camera's settings survive the trip back through createDevice",
    "[integration][device][enumerate]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    if(!protocolPresent(ctx, kCameraUuid))
      SKIP("the Camera protocol is not in this build");

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    REQUIRE(ctx.currentDocument() != nullptr);

    Console c;
    const auto found = enumerateVia(c, kCameraUuid);

    // The Camera protocol always offers "Default" and "Custom", so an empty
    // list is a broken enumerator, not absent hardware.
    INFO("enumerated " << found.size() << " camera identifiers");
    REQUIRE(!found.empty());

    // An identifier without a protocol or without protocol-specific settings is
    // already useless before createDevice ever sees it. Reported as one list
    // rather than one assertion per source: a laptop enumerates one identifier
    // per mode per pixel format per camera.
    const auto cameraKey
        = UuidKey<Device::ProtocolFactory>::fromString(QString{kCameraUuid});
    QStringList unusable;
    for(const auto& f : found)
      if(f.settings.protocol != cameraKey || !f.settings.deviceSpecificSettings.isValid())
        unusable += f.category + " / " + f.name;
    INFO("unusable identifiers: " << unusable.join(", ").toStdString());
    CHECK(unusable.isEmpty());

    // Round-trip fidelity on a representative spread rather than on all of
    // them: a laptop enumerates one identifier per mode per pixel format per
    // camera, which is hundreds of devices to create.
    std::vector<std::size_t> picks{0};
    if(found.size() > 1)
      picks.push_back(found.size() - 1);
    if(found.size() > 2)
      picks.push_back(found.size() / 2);

    for(std::size_t k : picks)
    {
      const auto& src = found[k];
      const auto want = settingsJson(src.settings);
      const QString devName = QString{"Input%1"}.arg(k);

      INFO(
          "from " << src.category.toStdString() << " / " << src.name.toStdString()
                  << ", enumerated settings: " << describe(want));
      REQUIRE(!want.isEmpty());

      c.eval(QString{"Score.createDevice(\"%1\", \"%2\", e.devices[%3].settings)"}
                 .arg(devName, kCameraUuid)
                 .arg(k));

      auto* dev = treeDevice(*doc, devName);
      REQUIRE(dev != nullptr);
      const auto got = settingsJson(dev->settings());
      INFO("device settings: " << describe(got));
      CHECK(got == want);
      CHECK(dev->settings().name == devName);
      CHECK(dev->settings().protocol == cameraKey);
    }

    // Usable, part 1: an inlet addressed to it resolves to this device in the
    // tree, which is the precondition for the gfx graph to route anything.
    QTemporaryDir shaderDir;
    REQUIRE(shaderDir.isValid());
    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto* imageInlet = isfImageInlet(c, interval, shaderDir);

    c.api->setAddress(imageInlet, "Input0:/");
    const auto addr = imageInlet->address().address;
    CHECK(addr.device == QStringLiteral("Input0"));
    auto* node = Device::try_getNodeFromAddress(explorerPlugin(*doc).rootNode(), addr);
    REQUIRE(node != nullptr);
    CHECK(node->get<Device::DeviceSettings>().name == QStringLiteral("Input0"));

    // Usable, part 2 -- hardware: an identifier that names a real capture path
    // must yield a device that actually opens and publishes the texture
    // parameter the gfx graph pulls from. "Default" and "Custom" name no path
    // (their input/device are "default"/""), so they cannot reach this, which
    // is why it is keyed on the JSON fields rather than on the name.
    std::optional<std::size_t> real;
    for(std::size_t i = 0; i < found.size(); i++)
    {
      const auto j = settingsJson(found[i].settings);
      const auto in = j["Input"].toString();
      const auto de = j["Device"].toString();
      if(!in.isEmpty() && !de.isEmpty() && de != QStringLiteral("default"))
      {
        real = i;
        break;
      }
    }
    if(!real)
      SKIP("no capture device enumerated: the hardware half of the round-trip "
           "cannot be checked on this machine");

    const auto want = settingsJson(found[*real].settings);
    INFO("real capture path: " << describe(want));
    c.eval(QString{"Score.createDevice(\"Live\", \"%1\", e.devices[%2].settings)"}
               .arg(kCameraUuid)
               .arg(*real));
    auto* live = treeDevice(*doc, "Live");
    REQUIRE(live != nullptr);
    CHECK(settingsJson(live->settings()) == want);

    if(!live->connected())
      SKIP("the enumerated capture device did not open (busy, or no permission)");

    auto* ossiaDev = live->getDevice();
    REQUIRE(ossiaDev != nullptr);
    // A gfx input device publishes exactly one thing: the texture parameter on
    // its root. Without it nothing can be pulled from the device and "usable"
    // would be a lie.
    CHECK(ossiaDev->get_root_node().get_parameter() != nullptr);
  });
}

// The three optional backends of the same table. Two of them enumerate
// asynchronously and none ships on Linux, so this skips by construction on most
// machines -- kept because the assertion is the part that is not obvious: an
// ASYNC backend's reported settings must round-trip through createDevice
// identically to a synchronous one's.
TEST_CASE(
    "NDI, Spout and Syphon enumerate into createDevice the same way",
    "[integration][device][enumerate]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    struct Backend
    {
      const char* name;
      const char* uuid;
    };
    const Backend backends[]
        = {{"NDI", kNdiUuid}, {"Spout", kSpoutUuid}, {"Syphon", kSyphonUuid}};

    bool anyPresent = false;
    for(auto& b : backends)
      anyPresent |= protocolPresent(ctx, b.uuid);
    if(!anyPresent)
      SKIP("none of NDI, Spout or Syphon is in this build");

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    Console c;
    int checked = 0;
    for(auto& b : backends)
    {
      if(!protocolPresent(ctx, b.uuid))
        continue;

      c.eval(QString{"var e = Score.enumerateDevices(\"%1\"); e.enumerate = true;"}
                 .arg(b.uuid));
      // Async discovery: give the backend the event-loop turns its queued
      // deviceAdded needs.
      spin(750);
      const int n = c.eval("e.devices.length").toInt();
      INFO(b.name << " enumerated " << n << " sources");
      if(n == 0)
        continue; // nothing publishing on this machine right now

      auto* ident = c.eval("e.devices[0]").toQObject();
      REQUIRE(ident != nullptr);
      const auto want
          = settingsJson(ident->property("settings").value<Device::DeviceSettings>());
      INFO(b.name << " enumerated settings: " << describe(want));
      REQUIRE(!want.isEmpty());

      const QString devName = QString{"In_%1"}.arg(b.name);
      c.eval(QString{"Score.createDevice(\"%1\", \"%2\", e.devices[0].settings)"}
                 .arg(devName, b.uuid));

      auto* dev = treeDevice(*doc, devName);
      REQUIRE(dev != nullptr);
      INFO(b.name << " device settings: " << describe(settingsJson(dev->settings())));
      CHECK(settingsJson(dev->settings()) == want);
      checked++;
    }

    if(checked == 0)
      SKIP("the built capture backends published no source to enumerate");
  });
}

// 2. The lifetime of what Score.enumerateDevices() hands QML.
//
// Two objects cross that boundary and neither of them is QML's to destroy.
//
// The enumerator: a QObject returned from a slot with no QObject parent gets
// QQmlEngine::JavaScriptOwnership, so the collector may delete it as soon as
// the script stops referring to it -- the same hazard EditJsContext::device()
// opts out of with QQmlEngine::setObjectOwnership(dev, CppOwnership).
// A script may keep the enumerator in a function-local `let` that stops
// referencing it as soon as the function returns, or in a component property.
// Collecting it runs ~GlobalDeviceEnumerator, which
// destroys the protocol-side enumerators still discovering asynchronously AND
// every identifier the script kept, so the context has to own it.
//
// The identifiers: the contract is "read enumerator.devices once; reuse
// dev.settings" -- a script stores identifiers and reads `.settings` off them
// much later, across re-enumerations, so a re-enumeration may not invalidate
// an identifier it handed out.
//
// Asserted here: enumerating from a dropped local gives the same answer as
// from a rooted reference; a dropped enumerator stays alive across garbage
// collection; a held identifier keeps its name and settings across a
// re-enumeration and while OTHER enumerators come and go; a re-enumeration
// does not lose or reorder sources; and repeating the cycle leaves the device
// layer working.
TEST_CASE(
    "collecting the enumerator QML was handed must not break enumeration",
    "[integration][device][enumerate]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    if(!protocolPresent(ctx, kCameraUuid))
      SKIP("the Camera protocol is not in this build");

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    Console c;

    // Baseline, with the enumerator rooted in the JS global scope.
    const auto baseline = enumerateVia(c, kCameraUuid);
    REQUIRE(!baseline.empty());
    c.eval("e = undefined;");

    // The dropped-local shape: the enumerator never escapes the function, and
    // the signal connection does not root it (QML roots the receiver of a
    // connection, not its sender).
    c.eval(
        QString{R"js(
      var seen = 0;
      function register() {
        let en = Score.enumerateDevices("%1");
        en.deviceAdded.connect(function() { seen++; });
        en.enumerate = true;
        return en.devices.length;
      }
    )js"}
            .arg(kCameraUuid));
    const int inFunction = c.eval("register()").toInt();
    CHECK(inFunction == (int)baseline.size());

    // The contract in full: "read enumerator.devices once; reuse
    // dev.settings". A script keeps the identifier and reads .settings much
    // later, so a captured identifier has to survive whatever else happens to
    // OTHER enumerators in the meantime.
    c.eval(QString{"var keep = Score.enumerateDevices(\"%1\");"
                   "keep.enumerate = true;"
                   "var held = keep.devices[0];"}
               .arg(kCameraUuid));
    {
      auto* ident = c.eval("held").toQObject();
      REQUIRE(ident != nullptr);
      CHECK(
          settingsJson(ident->property("settings").value<Device::DeviceSettings>())
          == settingsJson(baseline[0].settings));
    }

    // A second, unrelated enumerator is created and the script drops it --
    // exactly what a `let en = Score.enumerateDevices(...)` does when the
    // function returns.
    //
    // What score handed the script has to stay ALIVE: the enumerator owns the
    // protocol-side enumerators that are still discovering (NDI and Spout
    // report asynchronously) and every DeviceIdentifier the script kept, and
    // ~GlobalDeviceEnumerator destroys both. So it may not be the collector's
    // to take, which is the same ownership rule EditJsContext::device()
    // follows.
    auto probe = c.eval(QString{"Score.enumerateDevices(\"%1\")"}.arg(kCameraUuid));
    REQUIRE(probe.toQObject() != nullptr);
    REQUIRE(probe.toQObject()->inherits("JS::GlobalDeviceEnumerator"));
    CHECK(probe.toQObject()->parent() != nullptr);
    CHECK(QQmlEngine::objectOwnership(probe.toQObject()) == QQmlEngine::CppOwnership);
    QPointer<QObject> weak{probe.toQObject()};
    probe = QJSValue{};
    for(int i = 0; i < 8; i++)
    {
      c.engine.collectGarbage();
      spin(25);
    }
    CHECK(!weak.isNull());

    {
      auto* ident = c.eval("held").toQObject();
      REQUIRE(ident != nullptr);
      CHECK(
          settingsJson(ident->property("settings").value<Device::DeviceSettings>())
          == settingsJson(baseline[0].settings));
    }
    // The contract, in the one place it actually bites: the UI re-enumerates
    // (a backend switch, a refresh button, `enumerate` toggled off and on) and
    // THEN reads `.settings` off the identifier it stored before. The
    // identifier has to survive that.
    c.eval("keep.enumerate = false; keep.enumerate = true;");
    spin(50);
    {
      auto* ident = c.eval("held").toQObject();
      REQUIRE(ident != nullptr);
      CHECK(
          settingsJson(ident->property("settings").value<Device::DeviceSettings>())
          == settingsJson(baseline[0].settings));
      CHECK(ident->property("name").toString() == baseline[0].name);
    }
    // And a re-enumeration is not allowed to lose or reorder sources either:
    // the freshly read list is the same list.
    CHECK(signature(readDevices(c, "keep")) == signature(baseline));
    c.eval("keep = undefined; held = undefined;");

    // The collection must not have taken the protocol-side enumerators with
    // it: a new enumerator sees exactly what the first one saw.
    CHECK(signature(enumerateVia(c, kCameraUuid)) == signature(baseline));
    c.eval("e = undefined;");

    // ... and repeatedly, the way a user clicking through backends does. Each
    // cycle allocates a fresh enumerator, runs a full reprocess(), and then
    // drops the object for the collector.
    for(int i = 0; i < 8; i++)
    {
      INFO("cycle " << i);
      const int n = c.eval("register()").toInt();
      CHECK(n == (int)baseline.size());
      c.engine.collectGarbage();
      spin(10);
    }

    // The device layer is still usable afterwards: the point of the loop is
    // that nothing above corrupted it.
    c.eval(QString{"Score.createDevice(\"After\", \"%1\", {})"}.arg(kCameraUuid));
    CHECK(treeDevice(*doc, "After") != nullptr);
  });
}

// 3. ADDING a device while the transport plays.
//
// The one device-tree edit the domain contract permits, and the one a UI
// actually performs mid-playback, reached because the removeDevice above it is
// refused during execution.
//
// "It did not crash" is not the oracle. The added device has to be LIVE: its
// protocol is started by Execution::DocumentPlugin::registerDevice when the
// device arrives during execution, and a value pushed to one of its addresses
// has to leave the process. The receiver is a real UDP socket, so the
// assertion is bytes on the wire.
//
// And the transport has to be undisturbed, which is asserted as a play position
// that was still advancing after the add -- not merely isPlaying(), which
// stays true through a stalled engine.
TEST_CASE(
    "adding a device while the transport plays keeps playing and works",
    "[integration][device][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* exec = doc->context().findPlugin<Execution::DocumentPlugin>();
    REQUIRE(exec != nullptr);
    auto& transport = exec->executionController().transport();

    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();

    QTemporaryDir shaderDir;
    REQUIRE(shaderDir.isValid());

    QUdpSocket sink;
    REQUIRE(sink.bind(QHostAddress::LocalHost, 0));
    const quint16 sinkPort = sink.localPort();

    Console c;
    auto* imageInlet = isfImageInlet(c, interval, shaderDir);
    REQUIRE(imageInlet != nullptr);

    // Let the application settle before playing. Creating a document emits
    // View::activeDocumentChanged, whose QUEUED handler runs
    // DocumentManager::prepareNewDocument ->
    // ScenarioApplicationPlugin::prepareNewDocument, which TRIGGERS the Stop
    // action -- itself a queued connection to
    // ExecutionController::request_stop. Two queued hops: playing before both
    // have landed gets the score stopped from under the case a moment later.
    spin(500);

    StopOnExit stop{transport};

    c.api->play();
    REQUIRE(eventually([&] { return exec->isPlaying(); }));

    // The transport must be genuinely running, not merely flagged: the position
    // has to move before the edit, so that "it moved after the edit too" means
    // something.
    REQUIRE(eventually([&] { return interval.duration.playPercentage() > 0.; }));
    REQUIRE(interval.executing());
    const double beforeAdd = interval.duration.playPercentage();

    const int devicesBefore = (int)explorerPlugin(*doc).list().devices().size();

    // The permitted edit, exactly as a UI spells it.
    c.eval(QString{"Score.createOSCDevice(\"Sink\", \"127.0.0.1\", %1, %2)"}
               .arg(sinkPort + 1)
               .arg(sinkPort));
    c.eval("Score.createAddress(\"Sink:/val\", \"int\")");

    // Present, addressable, connected -- while still playing.
    CHECK(exec->isPlaying());
    auto* dev = treeDevice(*doc, "Sink");
    REQUIRE(dev != nullptr);
    CHECK((int)explorerPlugin(*doc).list().devices().size() == devicesBefore + 1);
    CHECK(dev->connected());

    // The address exists where it has to exist for the device to carry values:
    // in the ossia device the protocol owns.
    REQUIRE(dev->getDevice() != nullptr);
    auto* param = ossia::net::find_node(dev->getDevice()->get_root_node(), "/val");
    REQUIRE(param != nullptr);
    CHECK(param->get_parameter() != nullptr);
    // ... and it reaches score's own tree too, so the UI and setAddress can see
    // it. Asynchronously: NodeUpdateProxy::addAddress only touches the device
    // implementation, and the explorer node arrives through the device's
    // learning-mode node-created callback.
    REQUIRE(eventually([&] {
      return Device::try_getNodeFromString(
                 explorerPlugin(*doc).rootNode(), QStringList{"Sink", "val"})
             != nullptr;
    }));

    // Functional: the address created on the mid-playback device carries a
    // value out of the process.
    bool got = false;
    QByteArray payload;
    for(int attempt = 0; attempt < 20 && !got; attempt++)
    {
      dev->sendMessage(State::Address{"Sink", {"val"}}, 77 + attempt);
      if(eventually([&] { return sink.hasPendingDatagrams(); }, 250))
      {
        payload = sink.receiveDatagram().data();
        got = true;
      }
    }
    INFO("datagram: " << payload.toStdString());
    CHECK(got);
    CHECK(payload.contains("/val"));

    // Playback undisturbed: still playing, and the position kept moving across
    // the add.
    CHECK(exec->isPlaying());
    REQUIRE(eventually(
        [&] { return interval.duration.playPercentage() > beforeAdd; }, 3000));
    CHECK(interval.executing());

    // The device the tree knows at the end is still the one that was added.
    CHECK(treeDevice(*doc, "Sink") == dev);
  });
}

// 3b. ADDING a device whose name is already taken, while the transport plays.
//
// This is the state a backend switch lands in mid-playback: startMacro /
// removeDevice("Camera") -- REFUSED during execution -- /
// createDevice("Camera", ...) / setAddress(port, "Camera:/"). The create is the
// one half that goes through, and it goes through under a name that is still
// taken.
//
// A name is the only handle the document has on a device: "Camera:/" on a
// port, Score.device("Camera"), the explorer row. A second device under the
// same name does not replace the first, it shadows it -- lookups resolve to
// whichever comes first and the shadowed device's addresses stay live and
// unreachable -- so the create has to be refused, loudly.
//
// Refused on the NAME, not on the transport: adding a device during playback
// is the one device-tree edit the contract allows, and the second half of this
// case is that a legitimate add still lands while the score runs.
TEST_CASE(
    "a device name already taken is refused rather than shadowed",
    "[integration][device][playback]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    if(!protocolPresent(ctx, kCameraUuid))
      SKIP("the Camera protocol is not in this build");

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* exec = doc->context().findPlugin<Execution::DocumentPlugin>();
    REQUIRE(exec != nullptr);
    auto& transport = exec->executionController().transport();
    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();

    Console c;
    const auto found = enumerateVia(c, kCameraUuid);
    REQUIRE(found.size() >= 2);

    // Two modes whose settings differ, so "the first device is untouched" is
    // able to fail.
    const auto settingsA = settingsJson(found[0].settings);
    std::size_t b = 0;
    for(std::size_t i = 1; i < found.size(); i++)
    {
      if(settingsJson(found[i].settings) != settingsA)
      {
        b = i;
        break;
      }
    }
    if(b == 0)
      SKIP("the Camera protocol enumerated no two distinguishable modes");

    // See the note in case 3: play() too early gets stopped from under us.
    spin(500);
    StopOnExit stop{transport};

    c.api->play();
    REQUIRE(eventually([&] { return exec->isPlaying(); }));
    REQUIRE(eventually([&] { return interval.duration.playPercentage() > 0.; }));
    REQUIRE(interval.executing());
    const double beforeAdd = interval.duration.playPercentage();

    const int devicesBefore = (int)explorerPlugin(*doc).list().devices().size();

    // The permitted edit: an ADD, mid-playback.
    c.eval(QString{"Score.createDevice(\"Camera\", \"%1\", e.devices[0].settings)"}
               .arg(kCameraUuid));
    auto* first = treeDevice(*doc, "Camera");
    REQUIRE(first != nullptr);
    REQUIRE(settingsJson(first->settings()) == settingsA);

    // The collision, in the sequence a UI writes: the remove was refused, the
    // create follows anyway.
    {
      WarningCatcher warn;
      c.eval(QString{"Score.createDevice(\"Camera\", \"%1\", e.devices[%2].settings)"}
                 .arg(kCameraUuid)
                 .arg(b));

      // Loudly: silently doing nothing would leave the app believing it
      // switched backend.
      const auto said = WarningCatcher::joined();
      INFO("warnings: " << said);
      CHECK(warn.said("Camera"));
    }

    // And nothing shadowed: one device of that name, still the first one,
    // still carrying the settings it was created with.
    int named = 0;
    for(auto& n : explorerPlugin(*doc).rootNode().children())
      if(n.is<Device::DeviceSettings>() && n.displayName() == QStringLiteral("Camera"))
        named++;
    CHECK(named == 1);
    CHECK((int)explorerPlugin(*doc).list().devices().size() == devicesBefore + 1);
    CHECK(treeDevice(*doc, "Camera") == first);
    CHECK(settingsJson(first->settings()) == settingsA);

    // The refusal is about the name only: a legitimate add during playback
    // still lands. A playback guard on createDevice would break exactly this,
    // which is the permitted case.
    c.eval(QString{"Score.createDevice(\"Camera2\", \"%1\", e.devices[%2].settings)"}
               .arg(kCameraUuid)
               .arg(b));
    auto* second = treeDevice(*doc, "Camera2");
    REQUIRE(second != nullptr);
    CHECK(settingsJson(second->settings()) == settingsJson(found[b].settings));
    CHECK((int)explorerPlugin(*doc).list().devices().size() == devicesBefore + 2);

    // Playback undisturbed by either the refusal or the add.
    CHECK(exec->isPlaying());
    REQUIRE(eventually(
        [&] { return interval.duration.playPercentage() > beforeAdd; }, 3000));
    CHECK(interval.executing());
  });
}

// 4. Backend switching BETWEEN plays, as a UI writes it: startMacro /
//    removeDevice / createDevice / setAddress / endMacro.
//
// Two properties, each of which a plausible regression breaks on its own:
//   * ONE undo step for the whole switch. Each call inside goes through
//     EditJsContext::macro(), which joins the open macro instead of opening its
//     own; if any of them stopped doing that, one combobox change would need
//     three Ctrl-Z, and the intermediate states -- a document with no input
//     device, or an input device nothing is addressed to -- would become
//     reachable by undo.
//   * that one step restores the PREVIOUS device exactly: same protocol-
//     specific settings, same addressability.
TEST_CASE(
    "a backend switch wrapped in startMacro/endMacro is one undo step",
    "[integration][device][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    if(!protocolPresent(ctx, kCameraUuid))
      SKIP("the Camera protocol is not in this build");

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    Console c;
    const auto found = enumerateVia(c, kCameraUuid);
    REQUIRE(found.size() >= 2);

    // Two identifiers whose settings genuinely differ, so "restored exactly"
    // is able to fail.
    const auto json0 = settingsJson(found[0].settings);
    std::size_t b = 0;
    for(std::size_t i = 1; i < found.size(); i++)
    {
      if(settingsJson(found[i].settings) != json0)
      {
        b = i;
        break;
      }
    }
    if(b == 0)
      SKIP("the Camera protocol enumerated no two distinguishable modes");

    const auto settingsA = json0;
    const auto settingsB = settingsJson(found[b].settings);
    INFO("A = " << describe(settingsA) << "\nB = " << describe(settingsB));

    QTemporaryDir shaderDir;
    REQUIRE(shaderDir.isValid());
    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    auto* imageInlet = isfImageInlet(c, interval, shaderDir);

    // First backend, and the inlet routed to it: the state the user's next
    // combobox change has to be undoable back to.
    c.eval(QString{"Score.startMacro();"
                   "Score.createDevice(\"Input\", \"%1\", e.devices[0].settings);"
                   "Score.endMacro();"}
               .arg(kCameraUuid));
    c.api->setAddress(imageInlet, "Input:/");
    REQUIRE(treeDevice(*doc, "Input") != nullptr);
    REQUIRE(settingsJson(treeDevice(*doc, "Input")->settings()) == settingsA);
    REQUIRE(imageInlet->address().address.device == QStringLiteral("Input"));

    score::CommandStack& stack = doc->commandStack();
    const int before = stack.size();

    // The switch, in the shape a UI writes it. setAddress goes through the C++ entry
    // point of the very same object so the inlet under test is the one that
    // moves; it joins the open macro exactly like the two device calls, which
    // is what the "one undo step" assertion below covers.
    c.eval(QString{"Score.startMacro();"
                   "Score.removeDevice(\"Input\");"
                   "Score.createDevice(\"Input\", \"%1\", e.devices[%2].settings);"}
               .arg(kCameraUuid)
               .arg(b));
    c.api->setAddress(imageInlet, "Input:/");
    c.eval("Score.endMacro();");

    // The switch landed.
    auto* switched = treeDevice(*doc, "Input");
    REQUIRE(switched != nullptr);
    INFO("switched = " << describe(settingsJson(switched->settings())));
    CHECK(settingsJson(switched->settings()) == settingsB);

    // Exactly one device of that name: a switch that shadowed instead of
    // replacing would leave two, and the explorer would then resolve "Input:/"
    // to whichever came first.
    int named = 0;
    for(auto& n : explorerPlugin(*doc).rootNode().children())
      if(n.is<Device::DeviceSettings>() && n.displayName() == QStringLiteral("Input"))
        named++;
    CHECK(named == 1);

    // ONE undo step for remove + create + setAddress.
    CHECK(stack.size() == before + 1);

    stack.undo();
    spin(50);

    // Back to backend A, byte-for-byte, and still addressable.
    auto* restored = treeDevice(*doc, "Input");
    REQUIRE(restored != nullptr);
    INFO("restored = " << describe(settingsJson(restored->settings())));
    CHECK(settingsJson(restored->settings()) == settingsA);
    CHECK(imageInlet->address().address.device == QStringLiteral("Input"));
    CHECK(
        Device::try_getNodeFromAddress(
            explorerPlugin(*doc).rootNode(), imageInlet->address().address)
        != nullptr);

    // And forward again: redo is the same one step.
    stack.redo();
    spin(50);
    auto* redone = treeDevice(*doc, "Input");
    REQUIRE(redone != nullptr);
    CHECK(settingsJson(redone->settings()) == settingsB);
    CHECK(stack.size() == before + 1);
  });
}
