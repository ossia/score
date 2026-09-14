// Protocols.* driven the way a spatialisation-control interface drives it:
// from the console engine of JS::ApplicationPlugin, not from a Mapper device.
//
// Such an app is `score --ui qml/Main.qml`: afterStartup() builds a QQmlComponent
// on JS::ApplicationPlugin::m_consoleEngine and creates it. `Protocols` is a
// property of that engine's global object, backed by an
// ossia::qt::qml_protocols over the plugin's OWN network_context, run by the
// plugin's own std::thread -- not a Mapper's, and with no device tree anywhere
// in the picture.
//
// MinimalApplication loads that plugin, so these tests take the real
// JS::ApplicationPlugin out of the application context and instantiate the QML
// in the real m_consoleEngine, with the real asio thread. The single difference
// from a `--ui` run is that afterStartup() would then hand a QQuickItem root to
// a QQuickWindow; the app's root is an ApplicationWindow, which that code path
// does not adopt either, and the QML below has a QtObject root so the test
// needs no scene graph. Same engine, same globals, same io_context, same
// thread.
//
// The QML under test has the structure such an app has -- a Main.qml root
// holding the window properties the engine script writes to, plus
// `import "./Engine.js" as Engine` -- and the socket lifetime and the
// message mapping below are the ones a spatialisation controller drives
// Protocols.* with. Left out: logging, QSettings, the ListModel mirror, and
// parseADMInput (the /adm/obj/... input state machine, which no scenario here
// drives; `/adm/lis/...` takes the verbatim-forward path instead).
//
// The peers are QUdpSockets, and the OSC oracle is the hand-built encoder
// below: every wire assertion compares whole datagrams against bytes this file
// lays out itself, so a change in libossia's encoder cannot move the goalposts.

#include <score_test/App.hpp>

#include <JS/ApplicationPlugin.hpp>

#include <QElapsedTimer>
#include <QFile>
#include <QNetworkDatagram>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QThread>
#include <QUdpSocket>
#include <QtEndian>

#include <catch2/catch_all.hpp>

#include <cmath>
#include <cstring>
#include <vector>

namespace
{
// The OSC oracle: an independent encoder, with padded OSC strings and
// network-order IEEE-754.
QByteArray oscPad(QByteArray b)
{
  b.append('\0');
  while(b.size() % 4)
    b.append('\0');
  return b;
}

struct oscArg
{
  char tag;
  QByteArray payload;
};

oscArg oscF(double v)
{
  const float f = float(v);
  quint32 bits{};
  std::memcpy(&bits, &f, 4);
  QByteArray d(4, '\0');
  qToBigEndian(bits, d.data());
  return {'f', d};
}

oscArg oscI(qint32 v)
{
  QByteArray d(4, '\0');
  qToBigEndian(v, d.data());
  return {'i', d};
}

oscArg oscS(const QByteArray& v)
{
  return {'s', oscPad(v)};
}

QByteArray oscMessage(const QByteArray& address, const std::vector<oscArg>& args)
{
  QByteArray out = oscPad(address);
  QByteArray tags = ",";
  for(const auto& a : args)
    tags.append(a.tag);
  out += oscPad(tags);
  for(const auto& a : args)
    out += a.payload;
  return out;
}

// Decoder, for failure messages only: a byte diff of two OSC packets is
// unreadable, and every assertion below is on the bytes themselves.
QString oscDescribe(const QByteArray& packet)
{
  auto readString = [&](int& pos) -> QByteArray {
    const int start = pos;
    while(pos < packet.size() && packet[pos] != '\0')
      pos++;
    QByteArray s = packet.mid(start, pos - start);
    pos = ((pos - start + 4) / 4) * 4 + start;
    return s;
  };
  int pos = 0;
  QString out = QString::fromUtf8(readString(pos));
  if(pos >= packet.size())
    return out + " <no type tags>";
  const QByteArray tags = readString(pos);
  out += " " + QString::fromUtf8(tags);
  for(int i = 1; i < tags.size(); i++)
  {
    if(pos + 4 > packet.size() && tags[i] != 's')
      return out + " <truncated>";
    switch(tags[i])
    {
      case 'f': {
        const quint32 bits = qFromBigEndian<quint32>(packet.constData() + pos);
        float f{};
        std::memcpy(&f, &bits, 4);
        out += QStringLiteral(" %1").arg(double(f), 0, 'g', 9);
        pos += 4;
        break;
      }
      case 'i':
        out += QStringLiteral(" %1").arg(qFromBigEndian<qint32>(packet.constData() + pos));
        pos += 4;
        break;
      case 's':
        out += " \"" + QString::fromUtf8(readString(pos)) + "\"";
        break;
      default:
        return out + " <unhandled tag>";
    }
  }
  return out;
}

template <typename F>
bool spin(F pred, int ms = 5000)
{
  QElapsedTimer timer;
  timer.start();
  do
  {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    if(pred())
      return true;
    QThread::msleep(2);
  } while(timer.elapsed() < ms);
  return pred();
}

void pump(int ms)
{
  spin([] { return false; }, ms);
}

void bindUdp(QUdpSocket& socket)
{
  REQUIRE(socket.bind(QHostAddress{QHostAddress::LocalHost}, quint16{0}));
}

//! A port nothing holds: bind it, note it, release it.
quint16 freePort()
{
  QUdpSocket reservation;
  bindUdp(reservation);
  const auto p = reservation.localPort();
  reservation.close();
  return p;
}

QByteArray receive(QUdpSocket& socket)
{
  REQUIRE(spin([&] { return socket.hasPendingDatagrams(); }));
  return socket.receiveDatagram().data();
}

void expectDatagram(QUdpSocket& socket, const QByteArray& expected)
{
  const auto got = receive(socket);
  INFO("expected: " << oscDescribe(expected).toStdString());
  INFO("received: " << oscDescribe(got).toStdString());
  REQUIRE(got.toHex(' ').toStdString() == expected.toHex(' ').toStdString());
}

void expectSilence(QUdpSocket& socket, int ms = 400)
{
  const bool anything = spin([&] { return socket.hasPendingDatagrams(); }, ms);
  if(anything)
  {
    INFO("unexpected: " << oscDescribe(socket.receiveDatagram().data()).toStdString());
    FAIL("a datagram arrived where none was expected");
  }
}

void sendUdp(QUdpSocket& socket, quint16 target, const QByteArray& data)
{
  REQUIRE(socket.writeDatagram(data, QHostAddress::LocalHost, target) == data.size());
}

// The engine script: the socket lifetime and the wire mapping a
// spatialisation controller sends, except where marked.
const char* engine_js = R"JS(
const HALF_PI = Math.PI / 2.;

// Copied with the logging calls dropped, and the observables the test reads
// added at the top.
function onInputValueReceived(address, value) {
    inCount = inCount + 1;
    lastAddress = address;
    lastIsArray = Array.isArray(value);
    lastArgTypes = value.map(function(v) { return typeof v; }).join(",");
    lastArgs = JSON.stringify(value);

    let norm = null;
    if (address.startsWith("/spat/serv")) {
        norm = parseSpatGRISInput(value);
    }
    // (the /adm/obj/ branch and parseADMInput are out of scope here)

    if (norm) {
        lastCommand = norm.command;
        lastIndex = norm.sourceIndex;
        lastNormArgs = JSON.stringify(norm.args);

        for (let output of outputDevices) {
            if (!output.active || !output.udp) continue;

            const idx = norm.sourceIndex + (output.sourceIndexOffset || 0);
            const mapped = mapMessage(norm.command, idx, norm.args, output.type);
            if (!mapped || mapped.length === 0) continue;

            for (let msg of mapped) {
                // outboundUDP.osc() expects a JS array for the arguments —
                // wrap scalar values so "/a/b" with value 0 doesn't crash.
                const oscArgs = Array.isArray(msg.value) ? msg.value : [msg.value];
                output.udp.osc(msg.address, oscArgs);
            }
        }
        return;
    }

    if (address.startsWith("/adm/")) {
        forwardAdmRaw(address, value);
    }
}

function forwardAdmRaw(address, value) {
    const oscArgs = Array.isArray(value) ? value : [value];

    for (let output of outputDevices) {
        if (!output.active || !output.udp) continue;
        if (output.type !== "ADM-OSC") continue;

        let outAddress = address;
        const offset = output.sourceIndexOffset || 0;
        if (offset !== 0) {
            const m = address.match(/^\/adm\/obj\/(\d+)(\/.*)$/);
            if (m) {
                outAddress = `/adm/obj/${parseInt(m[1]) + offset}${m[2]}`;
            }
        }

        output.udp.osc(outAddress, oscArgs);
    }
}

function parseSpatGRISInput(value) {
    if (!value || value.length < 2) return null;

    if (typeof value[0] === "number") {
        // Legacy format: [sourceIndex, az, el, hspan, vspan, radius, reserved]
        if (value.length < 7) return null;
        return {
            command: "pol",
            sourceIndex: 1+value[0],
            args: [value[1], HALF_PI - value[2], value[5], value[3] / 2., value[4] * 2.]
        };
    }
    return {
        command: value[0],
        sourceIndex: (typeof value[1] === "number") ? value[1] : -1,
        args: value.slice(2)
    };
}

function mapMessage(command, idx, args, outputType) {
    switch (outputType) {
    case "SpatGRIS":       return mapForSpatGRIS(command, idx, args);
    case "ADM-OSC":        return mapForADM(command, idx, args);
    case "SPAT Revolution": return mapForSPAT(command, idx, args);
    }
    return [];
}

function mapForSpatGRIS(command, idx, args) {
    switch (command) {
    case "pol":
    case "deg":
    case "car":
        if (args.length < 5) return [];
        return [{
            address: "/spat/serv",
            value: [command, idx, args[0], args[1], args[2], args[3], args[4]]
        }];
    case "clr":
        return [{ address: "/spat/serv", value: ["clr", idx] }];
    case "alg":
        if (args.length < 1) return [];
        return [{ address: "/spat/serv", value: ["alg", idx, args[0]] }];
    }
    return [];
}

function mapForADM(command, idx, args) {
    switch (command) {
    case "pol":
        if (args.length < 5) return [];
        return polarToADM(idx, args[0], args[1], args[2], args[3]);
    case "deg":
        if (args.length < 5) return [];
        return polarToADM(idx,
                          args[0] * Math.PI / 180.0,
                          args[1] * Math.PI / 180.0,
                          args[2], args[3]);
    case "car":
        if (args.length < 5) return [];
        return cartesianToADM(idx, args[0], args[1], args[2], args[3]);
    case "clr":
        return [{ address: `/adm/obj/${idx}/xyz`, value: [0, 0, 0] }];
    case "alg":
        return [];
    }
    return [];
}

function polarToADM(idx, azimuthRad, elevationRad, radius, hspan) {
    const messages = [{
        address: `/adm/obj/${idx}/aed`,
        value: [
            -azimuthRad   * 180.0 / Math.PI,
             elevationRad * 180.0 / Math.PI,
             radius
        ]
    }];
    if (hspan !== undefined) {
        messages.push({ address: `/adm/obj/${idx}/w`, value: hspan });
    }
    return messages;
}

function cartesianToADM(idx, x, y, z, hspan) {
    const messages = [{
        address: `/adm/obj/${idx}/xyz`,
        value: [x, y, z]
    }];
    if (hspan !== undefined) {
        messages.push({ address: `/adm/obj/${idx}/w`, value: hspan });
    }
    return messages;
}

function mapForSPAT(command, idx, args) {
    switch (command) {
    case "pol":
        if (args.length < 5) return [];
        return polarToSPAT(idx, args[0], args[1], args[2], args[3], args[4]);
    case "deg":
        if (args.length < 5) return [];
        return polarToSPAT(idx,
                           args[0] * Math.PI / 180.0,
                           args[1] * Math.PI / 180.0,
                           args[2], args[3], args[4]);
    case "car":
        if (args.length < 5) return [];
        return cartesianToSPAT(idx, args[0], args[1], args[2], args[3], args[4]);
    case "clr":
        return [{ address: `/source/${idx}/xyz`, value: [0, 0, 0] }];
    case "alg":
        if (args.length < 1) return [];
        return [{
            address: `/source/${idx}/mode`,
            value: args[0] === "dome" ? "dome" : "panning"
        }];
    }
    return [];
}

function polarToSPAT(idx, azimuthRad, elevationRad, radius, hspan, vspan) {
    const messages = [{
        address: `/source/${idx}/aed`,
        value: [
            azimuthRad   * 180.0 / Math.PI,
            elevationRad * 180.0 / Math.PI,
            radius * 100
        ]
    }];
    if (hspan !== undefined && vspan !== undefined) {
        messages.push({
            address: `/source/${idx}/spread`,
            value: (hspan + vspan) / 2 * 100
        });
    }
    return messages;
}

function cartesianToSPAT(idx, x, y, z, hspan, vspan) {
    const messages = [{
        address: `/source/${idx}/xyz`,
        value: [x, y, z]
    }];
    if (hspan !== undefined && vspan !== undefined) {
        messages.push({
            address: `/source/${idx}/spread`,
            value: (hspan + vspan) / 2 * 100
        });
    }
    return messages;
}

function openOutputSocket(dev) {
    if (dev.udp) {
        try { dev.udp.close(); } catch (e) {}
        dev.udp = null;
    }
    try {
        dev.udp = Protocols.outboundUDP({
            Transport: { Host: dev.host, Port: dev.port },
            onError: function() {
                lastError = "output socket error on " + dev.name;
            }
        });
    } catch (e) {
        lastError = "failed to open outbound UDP: " + e;
        dev.udp = null;
    }
}

// Minus updateOutputList()/saveOutputDevices()
function createOutputDevice(name, host, port, type) {
    const dev = {
        name: name,
        host: host,
        port: port,
        type: type,
        active: true,
        sourceIndexOffset: 0,
        udp: null
    };
    openOutputSocket(dev);
    outputDevices.push(dev);
}

// Minus updateOutputList()/saveOutputDevices()
function removeOutputDevice(index) {
    if (index >= 0 && index < outputDevices.length) {
        const dev = outputDevices[index];
        if (dev.udp) {
            try { dev.udp.close(); } catch (e) {}
            dev.udp = null;
        }
        outputDevices.splice(index, 1);
    }
}

function closeInputDevice() {
    if (udpInput)
        udpInput.close();
    udpInput = null;
    oscInput = null;
    inputListening = false;
    inputPortError = "";
}

function createInputDevice(inputPort) {
    closeInputDevice();

    Qt.callLater(function () {
        oscInput = Protocols.osc({
            onOsc: function (a, v) {
                try {
                    onInputValueReceived(a, v);
                } catch (e) {
                    lastError = "onOsc: " + e;
                }
            }
        });
        udpInput = Protocols.inboundUDP({
            Transport: {
                Bind: "0.0.0.0",
                Port: inputPort
            },
            onMessage: function (bytes) {
                oscInput.processMessage(bytes);
            }
        });

        if (udpInput) {
            inputListening = true;
        } else {
            inputPortError = "Failed to open port " + inputPort + " (already in use?)";
        }
    });
}

// Test-only entry points, all of them app code paths reached from the UI.

// InputSection's Listen checkbox.
function listen(on, port) {
    if (on)
        createInputDevice(port);
    else
        closeInputDevice();
}

// Editing InputSection's port field closes the socket, then the checkbox
// binding recreates it.
function editPort(port) {
    closeInputDevice();
    createInputDevice(port);
}

// The app keeps no reference to a closed socket; hold one so the test can call
// osc() on it, which is what a stale `dev` in flight would do.
function closeOutputSocketKeepingReference(index) {
    staleSocket = outputDevices[index].udp;
    staleSocket.close();
}

function sendOnStaleSocket(address, value) {
    staleSocket.osc(address, value);
}

function reopenOutputSocket(index) {
    openOutputSocket(outputDevices[index]);
}
)JS";

const char* main_qml = R"QML(
import QtQml
import Score.UI as UI
import "./Engine.js" as Engine

QtObject {
    id: window

    // The window properties Engine.js writes to.
    property var outputDevices: []
    property var oscInput
    property var udpInput
    property string inputPortError: ""
    property bool inputListening: false

    // observables
    property int inCount: 0
    property string lastAddress: ""
    property string lastArgs: ""
    property string lastArgTypes: ""
    property bool lastIsArray: false
    property string lastCommand: ""
    property real lastIndex: -1
    property string lastNormArgs: ""
    property string lastError: ""
    property var staleSocket

    function listen(on, port) { Engine.listen(on, port); }
    function editPort(port) { Engine.editPort(port); }
    function addOutput(name, host, port, type) {
        Engine.createOutputDevice(name, host, port, type);
    }
    function removeOutput(index) { Engine.removeOutputDevice(index); }
    function closeOutputKeepingReference(index) {
        Engine.closeOutputSocketKeepingReference(index);
    }
    function sendOnStaleSocket(address, value) {
        Engine.sendOnStaleSocket(address, value);
    }
    function reopenOutput(index) { Engine.reopenOutputSocket(index); }
}
)QML";

//! The app, instantiated in the real console engine of JS::ApplicationPlugin.
struct console_app
{
  QTemporaryDir dir;
  std::unique_ptr<QQmlComponent> component;
  QObject* root{};

  explicit console_app(const score::GUIApplicationContext& ctx)
  {
    auto* plugin = ctx.findGuiApplicationPlugin<JS::ApplicationPlugin>();
    REQUIRE(plugin);
    REQUIRE(dir.isValid());
    write("Engine.js", engine_js);
    write("Main.qml", main_qml);

    // The `--ui` path: a QQmlComponent on the plugin's own console engine.
    component = std::make_unique<QQmlComponent>(
        &plugin->m_consoleEngine, QUrl::fromLocalFile(dir.filePath("Main.qml")));
    root = component->create();
    INFO(component->errorString().toStdString());
    REQUIRE(root);
  }

  ~console_app() { delete root; }

  void write(const QString& name, const char* body)
  {
    QFile f{dir.filePath(name)};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(body);
  }

  void call(const char* name)
  {
    REQUIRE(QMetaObject::invokeMethod(root, name));
    REQUIRE(error().isEmpty());
  }
  void call(const char* name, const QVariant& a)
  {
    REQUIRE(QMetaObject::invokeMethod(root, name, Q_ARG(QVariant, a)));
    REQUIRE(error().isEmpty());
  }
  void call(const char* name, const QVariant& a, const QVariant& b)
  {
    REQUIRE(QMetaObject::invokeMethod(root, name, Q_ARG(QVariant, a), Q_ARG(QVariant, b)));
    REQUIRE(error().isEmpty());
  }
  void call(
      const char* name, const QVariant& a, const QVariant& b, const QVariant& c,
      const QVariant& d)
  {
    REQUIRE(QMetaObject::invokeMethod(
        root, name, Q_ARG(QVariant, a), Q_ARG(QVariant, b), Q_ARG(QVariant, c),
        Q_ARG(QVariant, d)));
    REQUIRE(error().isEmpty());
  }

  QVariant get(const char* name) const { return root->property(name); }
  QString error() const { return root->property("lastError").toString(); }
  int count() const { return root->property("inCount").toInt(); }

  //! Listen on `port` and wait for the socket createInputDevice() defers with
  //! Qt.callLater to be in place.
  void startListening(quint16 port)
  {
    call("listen", true, int(port));
    REQUIRE(spin([&] { return get("inputListening").toBool(); }));
  }
};
}

TEST_CASE(
    "console engine parses OSC from an independent peer", "[console][protocols][osc]")
{
  score::test::run_in_app([&](const auto& ctx) {
    QUdpSocket peer;
    bindUdp(peer);
    const auto listenPort = freePort();

    console_app app{ctx};
    app.startListening(listenPort);

    SECTION("the [command, index, args...] form")
    {
      // A leading string selects the command, the second argument is the
      // source index and the rest is the payload.
      sendUdp(
          peer, listenPort,
          oscMessage(
              "/spat/serv",
              {oscS("pol"), oscF(3), oscF(0.5), oscF(0.25), oscF(0.75), oscF(0.4),
               oscF(0.2)}));

      REQUIRE(spin([&] { return app.count() == 1; }));
      REQUIRE(app.error().isEmpty());
      REQUIRE(app.get("lastAddress").toString() == "/spat/serv");
      // Array.isArray() is FALSE on this value: toScriptValue(QJSValueList)
      // hands the script a QV4 sequence wrapper rather than an Array. Not
      // asserted either way -- a real Array would be an improvement, not a
      // regression -- but the app branches on it, so the wrapper's behaviour
      // under .map/.slice/[]/length is what is pinned below, and the
      // consequence for the verbatim-forward path is asserted on the wire in
      // the outbound test.
      INFO("Array.isArray(args) = " << app.get("lastIsArray").toBool());
      // A string stays a string and the numbers are numbers: the whole
      // dispatch in parseSpatGRISInput keys on exactly this.
      REQUIRE(
          app.get("lastArgTypes").toString()
          == "string,number,number,number,number,number,number");
      REQUIRE(
          app.get("lastArgs").toString()
          == "[\"pol\",3,0.5,0.25,0.75,0.4000000059604645,0.20000000298023224]");
      REQUIRE(app.get("lastCommand").toString() == "pol");
      REQUIRE(app.get("lastIndex").toDouble() == 3.);
      REQUIRE(
          app.get("lastNormArgs").toString()
          == "[0.5,0.25,0.75,0.4000000059604645,0.20000000298023224]");
    }

    SECTION("an int32 index arrives as a JS number")
    {
      sendUdp(
          peer, listenPort,
          oscMessage(
              "/spat/serv",
              {oscS("clr"), oscI(7)}));

      REQUIRE(spin([&] { return app.count() == 1; }));
      REQUIRE(app.error().isEmpty());
      REQUIRE(app.get("lastArgTypes").toString() == "string,number");
      REQUIRE(app.get("lastCommand").toString() == "clr");
      REQUIRE(app.get("lastIndex").toDouble() == 7.);
      REQUIRE(app.get("lastNormArgs").toString() == "[]");
    }

    SECTION("the legacy seven-bare-numbers form")
    {
      // The legacy format: [sourceIndex, az, el, hspan, vspan, radius, _].
      sendUdp(
          peer, listenPort,
          oscMessage(
              "/spat/serv", {oscF(2), oscF(0.5), oscF(0.25), oscF(0.4), oscF(0.2),
                             oscF(0.75), oscF(0)}));

      REQUIRE(spin([&] { return app.count() == 1; }));
      REQUIRE(app.error().isEmpty());
      REQUIRE(
          app.get("lastArgTypes").toString()
          == "number,number,number,number,number,number,number");
      REQUIRE(app.get("lastCommand").toString() == "pol");
      REQUIRE(app.get("lastIndex").toDouble() == 3.);
      // HALF_PI - el, radius, hspan / 2, vspan * 2, computed in doubles from
      // the float32s that came off the wire.
      REQUIRE(
          app.get("lastNormArgs").toString()
          == "[0.5,1.3207963267948966,0.75,0.20000000298023224,0.4000000059604645]");
    }

    SECTION("several datagrams in a row are all delivered")
    {
      for(int i = 0; i < 64; i++)
        sendUdp(
            peer, listenPort,
            oscMessage("/spat/serv", {oscS("clr"), oscF(i)}));
      REQUIRE(spin([&] { return app.count() == 64; }));
      REQUIRE(app.error().isEmpty());
      REQUIRE(app.get("lastIndex").toDouble() == 63.);
    }
  });
}

TEST_CASE(
    "console engine sends each preset's OSC byte for byte",
    "[console][protocols][osc]")
{
  score::test::run_in_app([&](const auto& ctx) {
    QUdpSocket peer, spatgris, adm, revolution;
    bindUdp(peer);
    bindUdp(spatgris);
    bindUdp(adm);
    bindUdp(revolution);
    const auto listenPort = freePort();

    console_app app{ctx};
    app.startListening(listenPort);
    // TopMenu's three Quick Setup presets, on ports the test owns rather than
    // the fixed 18042 / 9000 / 8088.
    app.call("addOutput", "SpatGRIS_1", "127.0.0.1", int(spatgris.localPort()), "SpatGRIS");
    app.call("addOutput", "ADM_1", "127.0.0.1", int(adm.localPort()), "ADM-OSC");
    app.call(
        "addOutput", "SPAT_1", "127.0.0.1", int(revolution.localPort()),
        "SPAT Revolution");
    REQUIRE(app.error().isEmpty());

    // The values the app forwards are the float32s it read off the wire, so
    // every expectation is computed from float32 inputs in double precision,
    // exactly as the script does it.
    const double az = double(0.5f), el = double(0.25f), radius = double(0.75f);
    const double hspan = double(0.4f), vspan = double(0.2f);
    const auto polar = oscMessage(
        "/spat/serv",
        {oscS("pol"), oscF(3), oscF(az), oscF(el), oscF(radius), oscF(hspan),
         oscF(vspan)});

    SECTION("a polar command reaches all three protocols")
    {
      sendUdp(peer, listenPort, polar);

      // SpatGRIS speaks /spat/serv natively: the app re-emits it verbatim,
      // which means the datagram that goes out is the one that came in --
      // including the source index as a float32, because a JS number can only
      // ever leave as one.
      expectDatagram(spatgris, polar);
      // ...and that is this exact packet. Literal fixture, independent of the
      // encoder above: "/spat/serv" padded to 12, ",sffffff" padded to 12,
      // "pol\0", then 3.0, 0.5, 0.25, 0.75, 0.4, 0.2 as big-endian float32.
      REQUIRE(
          polar.toHex().toStdString()
          == "2f737061742f7365727600002c73666666666666000000007"
             "06f6c00404000003f0000003e8000003f4000003ecccccd3e4ccccd");

      // The ADM mapping: azimuth sign flipped, radians to degrees, hspan sent
      // separately as a bare scalar the script wraps.
      expectDatagram(
          adm, oscMessage(
                   "/adm/obj/3/aed",
                   {oscF(-az * 180.0 / M_PI), oscF(el * 180.0 / M_PI), oscF(radius)}));
      expectDatagram(adm, oscMessage("/adm/obj/3/w", {oscF(hspan)}));

      // The SPAT mapping: no sign flip, radius as a percentage, spread as the
      // mean of the two spans.
      expectDatagram(
          revolution,
          oscMessage(
              "/source/3/aed",
              {oscF(az * 180.0 / M_PI), oscF(el * 180.0 / M_PI), oscF(radius * 100)}));
      expectDatagram(
          revolution,
          oscMessage("/source/3/spread", {oscF((hspan + vspan) / 2 * 100)}));

      REQUIRE(app.error().isEmpty());
    }

    SECTION("a cartesian command reaches all three protocols")
    {
      const double x = double(0.5f), y = double(-0.25f), z = double(0.125f);
      sendUdp(
          peer, listenPort,
          oscMessage(
              "/spat/serv", {oscS("car"), oscF(4), oscF(x), oscF(y), oscF(z),
                             oscF(hspan), oscF(vspan)}));

      expectDatagram(
          spatgris,
          oscMessage(
              "/spat/serv", {oscS("car"), oscF(4), oscF(x), oscF(y), oscF(z),
                             oscF(hspan), oscF(vspan)}));
      expectDatagram(
          adm, oscMessage("/adm/obj/4/xyz", {oscF(x), oscF(y), oscF(z)}));
      expectDatagram(adm, oscMessage("/adm/obj/4/w", {oscF(hspan)}));
      expectDatagram(
          revolution, oscMessage("/source/4/xyz", {oscF(x), oscF(y), oscF(z)}));
      expectDatagram(
          revolution,
          oscMessage("/source/4/spread", {oscF((hspan + vspan) / 2 * 100)}));
      REQUIRE(app.error().isEmpty());
    }

    SECTION("clear and algorithm commands")
    {
      // "clr" is two arguments, and the second one is still a float32.
      sendUdp(peer, listenPort, oscMessage("/spat/serv", {oscS("clr"), oscF(3)}));
      expectDatagram(spatgris, oscMessage("/spat/serv", {oscS("clr"), oscF(3)}));
      expectDatagram(adm, oscMessage("/adm/obj/3/xyz", {oscF(0), oscF(0), oscF(0)}));
      expectDatagram(revolution, oscMessage("/source/3/xyz", {oscF(0), oscF(0), oscF(0)}));

      // "alg" carries a trailing string; ADM has no algorithm concept and must
      // stay silent.
      sendUdp(
          peer, listenPort,
          oscMessage("/spat/serv", {oscS("alg"), oscF(3), oscS("dome")}));
      expectDatagram(
          spatgris, oscMessage("/spat/serv", {oscS("alg"), oscF(3), oscS("dome")}));
      expectDatagram(revolution, oscMessage("/source/3/mode", {oscS("dome")}));
      expectSilence(adm);
      REQUIRE(app.error().isEmpty());
    }

    SECTION("an untranslatable /adm/ message is forwarded verbatim to ADM outputs only")
    {
      // The verbatim-forward path: the two floats must reach the ADM receiver
      // as two floats, this being where the script wraps the incoming argument
      // list with `Array.isArray(value) ? value : [value]`.
      sendUdp(peer, listenPort, oscMessage("/adm/lis/pos", {oscF(0.5), oscF(0.25)}));
      expectDatagram(adm, oscMessage("/adm/lis/pos", {oscF(0.5), oscF(0.25)}));
      expectSilence(spatgris);
      expectSilence(revolution);
      REQUIRE(app.error().isEmpty());
    }
  });
}

TEST_CASE(
    "console engine releases the inbound port on every Listen toggle and port edit",
    "[console][protocols][udp]")
{
  score::test::run_in_app([&](const auto& ctx) {
    QUdpSocket peer;
    bindUdp(peer);
    const auto first = freePort();
    const auto second = freePort();
    REQUIRE(first != second);

    console_app app{ctx};
    const auto ping = [&](quint16 target, int index) {
      sendUdp(peer, target, oscMessage("/spat/serv", {oscS("clr"), oscF(index)}));
    };

    app.startListening(first);
    ping(first, 1);
    REQUIRE(spin([&] { return app.count() == 1; }));

    // InputSection's Listen checkbox, off and on again on the same port.
    // Three times: a listener leaked on any cycle keeps the port and keeps
    // delivering.
    for(int cycle = 0; cycle < 3; cycle++)
    {
      INFO("cycle " << cycle);
      const int before = app.count();
      app.call("listen", false, 0);

      // The port is free for an independent socket. No SO_REUSEADDR is set on
      // either side -- libossia only sets it for multicast -- so this bind
      // succeeding means the app's socket is really gone.
      QUdpSocket squatter;
      REQUIRE(spin([&] { return squatter.bind(QHostAddress{QHostAddress::LocalHost}, first); }));

      // And nothing of the app's is still listening: the datagram goes to the
      // squatter, and the script never sees it.
      ping(first, 100 + cycle);
      REQUIRE(!receive(squatter).isEmpty());
      REQUIRE(app.count() == before);

      squatter.close();
      app.startListening(first);
      ping(first, 200 + cycle);
      REQUIRE(spin([&] { return app.count() == before + 1; }));
      REQUIRE(app.get("lastIndex").toDouble() == 200. + cycle);
      REQUIRE(app.error().isEmpty());
    }

    // Editing InputSection's port closes the current socket, and the new one
    // is bound on the new port.
    const int before = app.count();
    app.call("editPort", int(second));
    REQUIRE(spin([&] { return app.get("inputListening").toBool(); }));

    QUdpSocket squatter;
    REQUIRE(spin([&] { return squatter.bind(QHostAddress{QHostAddress::LocalHost}, first); }));
    ping(first, 300);
    REQUIRE(!receive(squatter).isEmpty());
    REQUIRE(app.count() == before);

    ping(second, 301);
    REQUIRE(spin([&] { return app.count() == before + 1; }));
    REQUIRE(app.get("lastIndex").toDouble() == 301.);
    REQUIRE(app.get("inputPortError").toString().isEmpty());
    REQUIRE(app.error().isEmpty());
  });
}

TEST_CASE(
    "console engine outbound sockets stop sending once closed and send again once reopened",
    "[console][protocols][udp]")
{
  score::test::run_in_app([&](const auto& ctx) {
    QUdpSocket peer, target;
    bindUdp(peer);
    bindUdp(target);
    const auto listenPort = freePort();

    console_app app{ctx};
    app.startListening(listenPort);
    app.call("addOutput", "SpatGRIS_1", "127.0.0.1", int(target.localPort()), "SpatGRIS");

    const auto clear = [&](int index) {
      sendUdp(peer, listenPort, oscMessage("/spat/serv", {oscS("clr"), oscF(index)}));
    };
    const auto expectForwarded = [&](int index) {
      expectDatagram(target, oscMessage("/spat/serv", {oscS("clr"), oscF(index)}));
    };

    clear(1);
    expectForwarded(1);

    SECTION("a closed socket delivers nothing, through the script's own reference")
    {
      // openOutputSocket closes the socket before dropping it; a message still
      // in flight through the forwarding loop would reach that same object.
      app.call("closeOutputKeepingReference", 0);
      pump(250);

      for(int i = 0; i < 4; i++)
        app.call(
            "sendOnStaleSocket", "/stale/message",
            QVariant::fromValue(QVariantList{0.5, 0.25}));
      clear(2);
      expectSilence(target);
      REQUIRE(app.error().isEmpty());

      // ...and the engine is unharmed: reopening the same endpoint delivers
      // again, which is the host/port edit path.
      app.call("reopenOutput", 0);
      clear(3);
      expectForwarded(3);
    }

    SECTION("close and reopen repeatedly")
    {
      for(int i = 0; i < 5; i++)
      {
        INFO("cycle " << i);
        app.call("reopenOutput", 0);
        clear(10 + i);
        expectForwarded(10 + i);
      }
      REQUIRE(app.error().isEmpty());
    }

    SECTION("a removed output stops receiving")
    {
      // removeOutputDevice, which TopMenu's Clear All runs in a loop.
      app.call("removeOutput", 0);
      pump(250);
      clear(4);
      expectSilence(target);
      REQUIRE(app.error().isEmpty());
    }
  });
}
