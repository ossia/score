// What happens when a document references a process or protocol that this build
// does not have.
//
// Protocols and processes are registered conditionally inside plug-ins that ship
// everywhere -- Syphon and Spout are both compiled into score-plugin-gfx under
// #if -- so this is routine rather than exotic: a macOS document opened on
// Windows, or anything at all opened in the wasm build.
//
// Cases still marked as pinning current behaviour are ones no fix has landed for
// yet; they record what is lost, not what is wanted.

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/ProcessList.hpp>

#include <score/application/ApplicationComponents.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/command/CommandData.hpp>
#include <score/plugins/UuidKey.hpp>
#include <score/plugins/UuidKeySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/presenter/DocumentManager.hpp>

#include <score_test/App.hpp>

#include <catch2/catch_all.hpp>

namespace
{
// A protocol UUID that no build registers: stands in for Syphon-on-Windows.
constexpr auto absent_uuid = "11111111-2222-3333-4444-555555555555";

UuidKey<Device::ProtocolFactory> absentProtocol()
{
  return UuidKey<Device::ProtocolFactory>::fromString(QString{absent_uuid});
}

// Bytes as a build that *has* the protocol emits them: name, protocol key, a
// protocol-specific payload, then the trailing delimiter.
QByteArray settingsFromRicherBuild(const QString& name)
{
  QByteArray b;
  DataStreamReader r{&b};
  r.m_stream << name << absentProtocol();
  r.m_stream << QStringLiteral("host-only protocol payload");
  r.insertDelimiter();
  return b;
}
}

TEST_CASE("DeviceSettings DataStream reports the missing protocol", "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // The binary format writes protocol settings inline with no length prefix,
    // so a reader without the factory genuinely cannot skip them: the payload
    // is unrecoverable and the only honest outcome is a clear diagnostic. What
    // must NOT happen is the old behaviour, where the delimiter check landed
    // mid-payload and blamed the whole file for being corrupt.
    const QByteArray bytes = settingsFromRicherBuild("syphon-in");

    Device::DeviceSettings s;
    DataStreamWriter w{bytes};
    REQUIRE_THROWS_AS(w.writeTo(s), std::runtime_error);

    // The device and protocol are named, so the message can tell the user which
    // machine to open the document on.
    try
    {
      Device::DeviceSettings s2;
      DataStreamWriter w2{bytes};
      w2.writeTo(s2);
    }
    catch(const std::runtime_error& e)
    {
      const QString what = QString::fromStdString(e.what());
      CHECK(what.contains("syphon-in"));
      CHECK(what.contains(absent_uuid));
      CHECK(what.contains(".score"));
    }
  });
}

TEST_CASE("DeviceSettings DataStream round-trips when nobody has the protocol",
          "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // Symmetric case: the writer had no factory either, so it wrote no payload
    // and the delimiter follows the protocol key directly. Nothing is lost and
    // this must keep working -- it is how a device whose plug-in is absent on
    // *both* ends survives a local save/load.
    Device::DeviceSettings in;
    in.name = "syphon-in";
    in.protocol = absentProtocol();

    QByteArray bytes;
    {
      DataStreamReader r{&bytes};
      r.readFrom(in);
    }

    Device::DeviceSettings out;
    DataStreamWriter w{bytes};
    REQUIRE_NOTHROW(w.writeTo(out));
    CHECK(out.name == in.name);
    CHECK(out.protocol == in.protocol);
  });
}

TEST_CASE("DeviceSettings JSON preserves the settings of an absent protocol",
          "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const QByteArray json
        = QStringLiteral(R"({"Name":"syphon-in","Protocol":"%1",)"
                         R"("ServerName":"Resolume","AppName":"Arena","Rate":60})")
              .arg(absent_uuid)
              .toUtf8();

    auto doc = readJson(json);
    REQUIRE(!doc.HasParseError());

    Device::DeviceSettings s;
    JSONWriter w{doc};
    w.writeTo(s);

    CHECK(s.name == "syphon-in");
    CHECK(s.protocol == absentProtocol());
    // No factory, so nothing could be parsed into a typed settings object...
    CHECK(s.deviceSpecificSettings.isNull());
    // ...but the raw members are kept.
    CHECK_FALSE(s.opaqueSettings.isEmpty());

    // Saving from this build must reproduce what the authoring machine wrote,
    // so the device still works when the document goes back to a build that
    // has the protocol.
    JSONReader rd;
    rd.readFrom(s);

    auto out = readJson(rd.toByteArray());
    REQUIRE(!out.HasParseError());
    REQUIRE(out.IsObject());
    CHECK(out["Name"] == "syphon-in");
    CHECK(out["ServerName"] == "Resolume");
    CHECK(out["AppName"] == "Arena");
    CHECK(out["Rate"].GetInt() == 60);
    CHECK(out.MemberCount() == doc.MemberCount());
  });
}

TEST_CASE("Preserved settings survive repeated round-trips", "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // Machine B opening, re-saving and re-opening must not erode the payload:
    // nested objects and arrays have to come back byte-identical too.
    const QByteArray original
        = QStringLiteral(R"({"Name":"cam","Protocol":"%1","Nested":{"a":[1,2,3],)"
                         R"("b":null},"Flag":true,"Ratio":0.5})")
              .arg(absent_uuid)
              .toUtf8();

    QByteArray current = original;
    for(int i = 0; i < 3; i++)
    {
      auto doc = readJson(current);
      REQUIRE(!doc.HasParseError());

      Device::DeviceSettings s;
      JSONWriter{doc}.writeTo(s);

      JSONReader rd;
      rd.readFrom(s);
      current = rd.toByteArray();
    }

    auto first = readJson(original);
    auto last = readJson(current);
    REQUIRE(!last.HasParseError());
    CHECK(first == last);
  });
}

TEST_CASE("An unavailable command can be reported instead of aborting",
          "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // instantiateUndoCommand() aborts (debug) or throws (release) on an unknown
    // command, which is right for a local programming error but fatal when the
    // command arrived from a peer running a different build. The network
    // handlers use the checked form instead.
    score::CommandData cmd;
    cmd.parentKey = CommandGroupKey{"NoSuchCommandGroup"};
    cmd.commandKey = CommandKey{"NoSuchCommand"};

    CHECK(ctx.instantiateUndoCommandIfAvailable(cmd) == nullptr);
  });
}

TEST_CASE("Polymorphic DataStream payloads are length-delimited", "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // This is what makes opaque preservation possible for processes and ports:
    // readFromAbstract wraps each polymorphic object in its own QByteArray.
    Process::ControlInlet inlet{QStringLiteral("ctl"), Id<Process::Port>{0}, nullptr};

    QByteArray b;
    {
      DataStreamReader r{&b};
      r.readFrom(static_cast<const Process::Inlet&>(inlet));
    }

    DataStreamWriter w{b};
    QByteArray inner;
    w.m_stream >> inner;

    CHECK(inner.size() > 0);
    CHECK(w.m_stream.stream.atEnd()); // the blob was the whole message

    // And an unknown concrete key leaves the outer stream correctly positioned,
    // so loadMissing receives the complete sub-payload and the caller can carry
    // on reading the next object.
    QByteArray two;
    {
      DataStreamReader r{&two};
      r.readFrom(static_cast<const Process::Inlet&>(inlet));
      r.readFrom(static_cast<const Process::Inlet&>(inlet));
    }
    DataStreamWriter w2{two};
    QByteArray first, second;
    w2.m_stream >> first;
    w2.m_stream >> second;
    CHECK(first == second);
    CHECK(w2.m_stream.stream.atEnd());

    (void)ctx;
  });
}

TEST_CASE("Missing factories have no fallback", "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    const auto unknown
        = UuidKey<Process::ProcessModel>::fromString(QString{absent_uuid});

    // No layer factory. FullViewIntervalPresenter::setupSlot and
    // TemporalIntervalPresenter dereference this unchecked; LayerData asserts.
    auto& layers = ctx.interfaces<Process::LayerFactoryList>();
    CHECK(layers.findDefaultFactory(unknown) == nullptr);

    // No process fallback.
    auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
    CHECK(procs.get(unknown) == nullptr);

    // No port fallback either: writePorts passes SCORE_ABORT as its failure
    // callback, so a document using a plugin-provided port type aborts on load.
    auto& ports = ctx.interfaces<Process::PortFactoryList>();
    QByteArray empty;
    DataStreamWriter w{empty};
    CHECK(ports.loadMissing(w.toVariant(), nullptr) == nullptr);
  });
}

TEST_CASE("checkAndUpdateJson cannot see factories missing inside a present plugin",
          "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // A document whose Plugins array lists only plugins this build has, but
    // whose content references a protocol UUID it does not have. This is the
    // Syphon case: score_plugin_gfx is present on every platform, so the
    // plugin-level check cannot express "Syphon is missing".
    const QByteArray json
        = QStringLiteral(R"({"Version":%1,"Plugins":[],)"
                         R"("Device":{"Name":"syphon-in","Protocol":"%2"}})")
              .arg(ctx.applicationSettings.saveFormatVersion.value())
              .arg(absent_uuid)
              .toUtf8();

    auto doc = readJson(json);
    REQUIRE(!doc.HasParseError());

    // Reports the document as fully loadable.
    CHECK(score::DocumentManager::checkAndUpdateJson(doc, ctx));
  });
}
