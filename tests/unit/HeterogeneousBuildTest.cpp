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
#include <Process/OpaqueProcess.hpp>
#include <Process/ProcessList.hpp>

#include <score/application/ApplicationComponents.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/command/CommandData.hpp>
#include <score/plugins/UuidKey.hpp>
#include <score/plugins/UuidKeySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/presenter/DocumentManager.hpp>

#include <score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/model/path/Path.hpp>

#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <cstring>
#include <score/plugins/SerializableHelpers.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

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

TEST_CASE("A process with no factory keeps its identity and data", "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();

    auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
    REQUIRE_FALSE(procs.empty());

    // Serialize a real process, then pretend we are a build that does not have
    // its plug-in by renaming the factory it points at. That is exactly the
    // Syphon / VST situation: the bytes were written by a richer build.
    Process::ProcessModel* original{};
    for(auto& fac : procs)
    {
      original = fac.make(TimeVal::fromMsecs(1000), {}, Id<Process::ProcessModel>{7},
                          dctx, doc);
      if(original)
        break;
    }
    REQUIRE(original);
    const auto realKey = original->concreteKey();
    const auto inlets = original->inlets().size();
    const auto outlets = original->outlets().size();

    JSONReader r;
    r.readFrom(*original);
    auto authored = readJson(r.toByteArray());
    REQUIRE(authored.IsObject());
    REQUIRE(authored.HasMember("uuid"));
    authored["uuid"].SetString(absent_uuid, authored.GetAllocator());

    // What a plug-in of its own would have written. Without this the test
    // asserts that no data survives no data.
    auto& alloc = authored.GetAllocator();
    authored.AddMember("PluginState", "opaque-and-preserved", alloc);
    rapidjson::Value nested{rapidjson::kObjectType};
    nested.AddMember("depth", 3, alloc);
    nested.AddMember("ratio", 0.25, alloc);
    authored.AddMember("Nested", nested, alloc);

    auto* loaded = deserialize_interface(
        procs, JSONObject::Deserializer{authored}, dctx, doc);
    REQUIRE(loaded);

    auto* opaque = dynamic_cast<Process::OpaqueProcessModel*>(loaded);
    REQUIRE(opaque);

    // It reports the key of what it replaces, not one of its own: saving must
    // write the original UUID or the process is lost for everybody.
    CHECK(opaque->concreteKey() != realKey);
    CHECK(opaque->concreteKey()
          == UuidKey<Process::ProcessModel>::fromString(QString{absent_uuid}));

    // Ports were rebuilt rather than swallowed, so cables to this process still
    // resolve and its controls still hold values.
    CHECK_FALSE(opaque->portsAreOpaque());
    CHECK(opaque->inlets().size() == inlets);
    CHECK(opaque->outlets().size() == outlets);

    // And saving reproduces what the authoring machine wrote.
    JSONReader out;
    out.readFrom(*loaded);
    auto reserialized = readJson(out.toByteArray());
    REQUIRE(reserialized.IsObject());
    CHECK(reserialized == authored);

    // rapidjson's operator== ignores member order, so it cannot see a payload
    // that came back rearranged. Name the members that matter directly.
    REQUIRE(reserialized.HasMember("PluginState"));
    CHECK(reserialized["PluginState"] == "opaque-and-preserved");
    REQUIRE(reserialized.HasMember("Nested"));
    CHECK(reserialized["Nested"]["depth"].GetInt() == 3);
    CHECK(reserialized["Nested"]["ratio"].GetDouble() == 0.25);

    // Telling our members from the plug-in's is by name, so one that score
    // gains but the list does not know about would be captured *and* written
    // by the base: a duplicate key, copied again on every load. Counting
    // catches that; comparing values does not, since the first of a duplicate
    // pair reads back correctly.
    CHECK(reserialized.MemberCount() == authored.MemberCount());

    auto* again = deserialize_interface(
        procs, JSONObject::Deserializer{reserialized}, dctx, doc);
    REQUIRE(again);
    JSONReader third;
    third.readFrom(*again);
    auto thrice = readJson(third.toByteArray());
    CHECK(thrice.MemberCount() == authored.MemberCount());
  });
}

TEST_CASE("A stand-in survives being written in the other format", "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // A document read from .score is written to the binary format on every
    // autosave, and moving an interval serialises its processes to the binary
    // format and rebuilds them from those bytes. A payload that could only be
    // written in the format it arrived in would be lost by dragging a box.
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto& procs = ctx.interfaces<Process::ProcessFactoryList>();

    Process::ProcessModel* original{};
    for(auto& fac : procs)
    {
      original = fac.make(TimeVal::fromMsecs(1000), {}, Id<Process::ProcessModel>{31},
                          dctx, doc);
      if(original)
        break;
    }
    REQUIRE(original);

    JSONReader r;
    r.readFrom(*original);
    auto authored = readJson(r.toByteArray());
    authored["uuid"].SetString(absent_uuid, authored.GetAllocator());
    authored.AddMember("PluginState", "must survive both", authored.GetAllocator());

    auto* fromJson = deserialize_interface(
        procs, JSONObject::Deserializer{authored}, dctx, doc);
    REQUIRE(dynamic_cast<Process::OpaqueProcessModel*>(fromJson));

    // JSON in, binary out, binary in, JSON out.
    QByteArray binary;
    {
      DataStreamReader w{&binary};
      w.readFrom(static_cast<const Process::ProcessModel&>(*fromJson));
    }

    DataStreamWriter dw{binary};
    auto* fromBinary = deserialize_interface(procs, dw, dctx, doc);
    REQUIRE(fromBinary);
    auto* opaque = dynamic_cast<Process::OpaqueProcessModel*>(fromBinary);
    REQUIRE(opaque);
    CHECK(opaque->concreteKey()
          == UuidKey<Process::ProcessModel>::fromString(QString{absent_uuid}));

    JSONReader back;
    back.readFrom(*fromBinary);
    auto out = readJson(back.toByteArray());
    REQUIRE(out.IsObject());
    REQUIRE(out.HasMember("PluginState"));
    CHECK(out["PluginState"] == "must survive both");
  });
}

TEST_CASE("A port with no factory keeps its id and data", "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // VST and LV2 bring their own control port types along with the process, so
    // a build without them meets unknown ports as well as unknown processes.
    // This used to abort: writePorts had SCORE_ABORT as its failure path.
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();

    auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
    Process::ProcessModel* original{};
    for(auto& fac : procs)
    {
      auto* p = fac.make(TimeVal::fromMsecs(1000), {}, Id<Process::ProcessModel>{21},
                         dctx, doc);
      if(p && !p->inlets().empty())
      {
        original = p;
        break;
      }
    }
    REQUIRE(original);
    const auto portId = original->inlets().front()->id();

    JSONReader r;
    r.readFrom(*original);
    auto authored = readJson(r.toByteArray());
    REQUIRE(authored.HasMember("Inlets"));
    REQUIRE(authored["Inlets"].IsArray());
    REQUIRE(authored["Inlets"].Size() > 0);
    authored["Inlets"][0]["uuid"].SetString(absent_uuid, authored.GetAllocator());

    auto* loaded = deserialize_interface(
        procs, JSONObject::Deserializer{authored}, dctx, doc);
    REQUIRE(loaded);
    REQUIRE_FALSE(loaded->inlets().empty());

    auto* opaque = dynamic_cast<Process::OpaqueInlet*>(loaded->inlets().front());
    REQUIRE(opaque);

    // The id is what cables resolve against, so a stand-in that renumbered the
    // port would silently break every cable pointing at it.
    CHECK(opaque->id() == portId);
    CHECK(opaque->concreteKey()
          == UuidKey<Process::Port>::fromString(QString{absent_uuid}));

    JSONReader out;
    out.readFrom(*loaded);
    auto reserialized = readJson(out.toByteArray());
    CHECK(reserialized == authored);
  });
}

TEST_CASE("A port with no factory survives the binary format too", "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // Unlike a whole process, a port *can* be recovered from a binary payload:
    // deserialize_interface gives each one its own length-delimited blob, so
    // the tail after the key is exactly this port's data.
    Process::ControlInlet inlet{QStringLiteral("ctl"), Id<Process::Port>{42}, nullptr};

    QByteArray one;
    {
      DataStreamReader r{&one};
      r.readFrom(static_cast<const Process::Inlet&>(inlet));
    }

    // The blob is written as [quint32 length][16-byte key][data], so renaming
    // the factory is a patch in place -- this is what a build that *has* the
    // plug-in would have produced.
    REQUIRE(one.size() > 20);
    const auto absent = UuidKey<Process::Port>::fromString(QString{absent_uuid});
    std::memcpy(one.data() + 4, &absent.impl(), 16);

    QByteArray ports;
    {
      QDataStream s{&ports, QIODevice::WriteOnly};
      s << (int32_t)1;
      s.writeRawData(one.constData(), one.size());
      s << (int32_t)0;
    }

    Process::Inlets ins;
    Process::Outlets outs;
    DataStreamWriter w{ports};
    Process::writePorts(
        w, ctx.interfaces<Process::PortFactoryList>(), ins, outs, nullptr);

    REQUIRE(ins.size() == 1);
    auto* opaque = dynamic_cast<Process::OpaqueInlet*>(ins.front());
    REQUIRE(opaque);
    CHECK(opaque->id() == Id<Process::Port>{42});
    CHECK(opaque->concreteKey() == absent);

    // And it writes back exactly what it was given.
    QByteArray again;
    {
      DataStreamReader r{&again};
      r.readFrom(static_cast<const Process::Inlet&>(*opaque));
    }
    CHECK(again == one);

    qDeleteAll(ins);
    qDeleteAll(outs);
  });
}

TEST_CASE("A document plug-in with no factory keeps its data", "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // Document plug-ins carry whole subsystems' state -- the network add-on
    // keeps its groups in one -- and were dropped outright when absent, so
    // saving wrote the document back without them.
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    const QByteArray authored
        = QStringLiteral(R"({"uuid":"%1","Groups":["all","band"],"Tempo":120})")
              .arg(absent_uuid)
              .toUtf8();

    auto json = readJson(authored);
    REQUIRE(!json.HasParseError());

    auto& facs = ctx.interfaces<score::DocumentPluginFactoryList>();
    auto& dctx = const_cast<score::DocumentContext&>(doc->context());
    auto* loaded
        = deserialize_interface(facs, JSONObject::Deserializer{json}, dctx, doc);
    REQUIRE(loaded);

    auto* opaque = dynamic_cast<score::OpaqueDocumentPlugin*>(loaded);
    REQUIRE(opaque);
    CHECK(opaque->concreteKey()
          == UuidKey<score::DocumentPluginFactory>::fromString(QString{absent_uuid}));

    JSONReader out;
    out.readFrom(static_cast<const score::SerializableDocumentPlugin&>(*opaque));
    auto reserialized = readJson(out.toByteArray());
    CHECK(reserialized == json);

    delete loaded;
  });
}

TEST_CASE("An unclaimed process still gets a layer", "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // The interval presenters build header and footer delegates from this
    // without checking it, and LayerData asserts on it, so a process no factory
    // claims must still resolve to something displayable.
    auto& layers = ctx.interfaces<Process::LayerFactoryList>();
    const auto unknown
        = UuidKey<Process::ProcessModel>::fromString(QString{absent_uuid});

    // The fallback is registered and never wins a normal lookup: findDefaultFactory
    // iterates an unordered map, so one that took part in matching would shadow
    // real factories at random.
    auto* fallback = layers.fallbackFactory();
    REQUIRE(fallback);
    CHECK(fallback->isFallback());
    CHECK(layers.findDefaultFactory(unknown) == nullptr);

    // It is reached only through the process, and only for a stand-in. Ordinary
    // processes without a layer keep resolving to nothing, so that they are not
    // suddenly drawn in a slot.
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
    Process::ProcessModel* plain{};
    for(auto& fac : procs)
    {
      plain = fac.make(TimeVal::fromMsecs(1000), {}, Id<Process::ProcessModel>{11},
                       doc->context(), doc);
      if(plain)
        break;
    }
    REQUIRE(plain);
    if(auto* f = layers.findDefaultFactory(*plain))
      CHECK_FALSE(f->isFallback());
  });
}

TEST_CASE("The list of members owned by ProcessModel has not drifted",
          "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // OpaqueProcessModel tells its own members from the plug-in's by name. If
    // score gains or renames one and this list is not updated, the member is
    // written twice on save, or captured and then lost. Serialize a real
    // process and check the two agree.
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
    Process::ProcessModel* p{};
    for(auto& fac : procs)
    {
      p = fac.make(TimeVal::fromMsecs(1000), {}, Id<Process::ProcessModel>{9},
                   doc->context(), doc);
      if(p)
        break;
    }
    REQUIRE(p);

    JSONReader r;
    r.readFrom(*p);
    auto obj = readJson(r.toByteArray());
    REQUIRE(obj.IsObject());

    const auto& base = Process::OpaqueProcessModel::baseMemberNames();
    for(const auto& name : base)
    {
      INFO("ProcessModel is expected to write " << name.toStdString());
      CHECK(obj.HasMember(name.toUtf8().constData()));
    }
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

    // Nothing here can tell that Syphon is missing, and nothing could: the
    // check works on plug-in keys, and score_plugin_gfx is present. Which is
    // why the device itself has to preserve what it cannot parse.
    const auto check = score::DocumentManager::checkAndUpdateJson(doc, ctx);
    CHECK(check.loadable);
    CHECK(check.missingPlugins.empty());
  });
}

TEST_CASE("A document naming a plug-in we lack still opens", "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // Refusing outright made a document unopenable on any machine that did not
    // have every plug-in it mentions, which is every machine once builds differ
    // by platform. It opens now, and reports what is missing so the caller can
    // say so.
    const QByteArray json
        = QStringLiteral(R"({"Version":%1,"Plugins":[{"Key":"%2","Version":1}]})")
              .arg(ctx.applicationSettings.saveFormatVersion.value())
              .arg(absent_uuid)
              .toUtf8();

    auto doc = readJson(json);
    REQUIRE(!doc.HasParseError());

    const auto check = score::DocumentManager::checkAndUpdateJson(doc, ctx);
    CHECK(check.loadable);
    REQUIRE(check.missingPlugins.size() == 1);
    CHECK(check.missingPlugins.front()
          == UuidKey<score::Plugin>::fromString(QString{absent_uuid}));
  });
}

TEST_CASE("A document from a newer score is still refused", "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // The opposite case must keep failing: here the factory *is* found, and
    // would read data written in a format it does not know.
    const QByteArray json
        = QStringLiteral(R"({"Version":%1,"Plugins":[]})")
              .arg(ctx.applicationSettings.saveFormatVersion.value() + 1)
              .toUtf8();

    auto doc = readJson(json);
    REQUIRE(!doc.HasParseError());
    CHECK_FALSE(score::DocumentManager::checkAndUpdateJson(doc, ctx).loadable);
  });
}

TEST_CASE("A path does not resolve to an object of another type", "[heterogeneous]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // A path names an object by position and name, and a stand-in keeps the id
    // and name of what it replaces -- so a path written for the real type
    // resolves to it. Commands then write through that pointer:
    // Process::SetControlValue holds a Path<ControlInlet> and lives in a
    // library every build has, so a peer without the plug-in that provided the
    // port receives one and nothing else stands in the way.
    //
    // Provoked here with two ordinary types, since what is being checked is the
    // resolution and not the stand-in.
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();

    auto& model = doc->model().modelDelegate();
    auto& interval
        = safe_cast<Scenario::ScenarioDocumentModel&>(model).baseScenario().interval();

    const Path<Scenario::IntervalModel> right{interval};
    REQUIRE(right.try_find(dctx) == &interval);
    REQUIRE_NOTHROW(right.find(dctx));

    // The same position, asked for as something it is not.
    const Path<Process::ProcessModel> wrong{
        right.unsafePath(), Path<Process::ProcessModel>::UnsafeDynamicCreation{}};

    CHECK(wrong.try_find(dctx) == nullptr);
    CHECK_THROWS(wrong.find(dctx));
  });
}
