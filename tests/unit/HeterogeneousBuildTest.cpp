// What happens when a document references a process or protocol that this build
// does not have. See docs/remote-control-plan.md sections 5bis-5quater.
//
// These pin the *current* behaviour, which is wrong in several places: they are
// the baseline the opaque-preservation work is written against, and they will be
// inverted as each fix lands.
//
// Note: several of these failures are SIGTRAP in a debug build (SCORE_BREAKPOINT
// fires before the throw / inside SCORE_ASSERT), so the assertions below prove
// the underlying stream misalignment rather than invoking the crash.

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

TEST_CASE("DeviceSettings DataStream desyncs when the protocol is absent", "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const QByteArray bytes = settingsFromRicherBuild("syphon-in");

    // Replay exactly what DataStreamWriter::write(DeviceSettings&) does when
    // ProtocolFactoryList::get() returns null: read name and protocol, skip the
    // payload entirely, then expect the delimiter.
    DataStreamWriter w{bytes};
    QString name;
    UuidKey<Device::ProtocolFactory> protocol;
    w.m_stream >> name >> protocol;

    REQUIRE(name == "syphon-in");
    REQUIRE(protocol == absentProtocol());

    int32_t delimiter{};
    w.m_stream.stream >> delimiter;

    // The payload is not length-delimited, so it cannot be skipped: what the
    // reader lands on is the payload, not the delimiter. checkDelimiter() then
    // SIGTRAPs (debug) or throws "Corrupt save file." (release).
    CHECK(delimiter != int32_t(0xDEADBEEF));
  });
}

TEST_CASE("DeviceSettings JSON silently drops the payload", "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const QByteArray json
        = QStringLiteral(R"({"Name":"syphon-in","Protocol":"%1",)"
                         R"("ServerName":"Resolume","AppName":"Arena"})")
              .arg(absent_uuid)
              .toUtf8();

    auto doc = readJson(json);
    REQUIRE(!doc.HasParseError());

    Device::DeviceSettings s;
    JSONWriter w{doc};
    w.writeTo(s);

    CHECK(s.name == "syphon-in");
    CHECK(s.protocol == absentProtocol());
    // Gone, with no error surfaced to the caller.
    CHECK(s.deviceSpecificSettings.isNull());

    // So re-serializing writes a husk: the settings the host authored are
    // destroyed by a round-trip through a build lacking the protocol.
    JSONReader rd;
    rd.readFrom(s);
    const QString out = rd.toString();
    CHECK(out.contains("syphon-in"));
    CHECK_FALSE(out.contains("Resolume"));
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
