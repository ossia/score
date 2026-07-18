// Value-asserting unit tests for the score-lib-process data model:
// - Port hierarchy: construction, ids, types, addresses, values, domains
// - DataStream + JSON serialization round-trips of ports through the
//   polymorphic PortFactory path (the same path documents load through)
// - AudioOutlet gain/pan/propagate persistence, ComboBox alternatives
// - Cable / CableData round-trips and endpoint path stability
// - A minimal concrete ProcessModel: object-graph invariants (port lookup by
//   id, parenting) and full DataStream + JSON round-trip of the process
//   metadata (duration, position, size, loops) plus its ports
// - Corrupt / truncated port buffers load to nullptr without crashing
//
// Everything here is pure data model: no document, no execution engine.
// Value propagation through cables (outlet -> inlet at runtime) lives in the
// ossia execution graph and needs the engine: left for an integration round.

#include <State/Address.hpp>
#include <State/Domain.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/CableData.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortSerialization.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Process/TimeValueSerialization.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <ossia/network/domain/domain.hpp>

#include <catch2/catch_test_macros.hpp>

#include <csignal>
#include <memory>

namespace
{
// In SCORE_DEBUG builds, checkDelimiter() raises SIGTRAP (SCORE_BREAKPOINT)
// before throwing "Corrupt save file". Outside a debugger that trap would
// kill the process; the throw right after is the actual testable error path.
static const int g_ignore_sigtrap = [] {
  std::signal(SIGTRAP, SIG_IGN);
  return 0;
}();
// The serialization visitors resolve score::AppComponents() in their
// constructors, and score::Entity construction (ProcessModel is an Entity)
// reaches score::AppContext() through Skin::instance(). The stock
// MockApplication throws on context(), so provide a minimal application
// interface with a real, GUI-less ApplicationContext, plus the
// PortFactoryList registered exactly like real applications do.
struct TestApplication final : public score::ApplicationInterface
{
  score::ApplicationSettings appSettings;
  score::ApplicationComponentsData compData;
  score::ApplicationComponents comps{compData};
  score::DocumentList docList;
  std::vector<std::unique_ptr<score::SettingsDelegateModel>> settingsVec;
  score::ApplicationContext ctx{appSettings, comps, docList, settingsVec};
  Process::PortFactoryList* ports{};

  TestApplication()
  {
    appSettings.gui = false; // Skin::instance() -> NoGUI skin (no QFont use)
    m_instance = this;

    auto pl = std::make_unique<Process::PortFactoryList>();
    pl->insert(std::make_unique<Process::PortFactory_T<Process::ControlInlet>>());
    pl->insert(std::make_unique<Process::PortFactory_T<Process::ControlOutlet>>());
    pl->insert(std::make_unique<Process::PortFactory_T<Process::ValueInlet>>());
    pl->insert(std::make_unique<Process::PortFactory_T<Process::ValueOutlet>>());
    pl->insert(std::make_unique<Process::PortFactory_T<Process::AudioInlet>>());
    pl->insert(std::make_unique<Process::PortFactory_T<Process::AudioOutlet>>());
    pl->insert(std::make_unique<Process::PortFactory_T<Process::MidiInlet>>());
    pl->insert(std::make_unique<Process::PortFactory_T<Process::MidiOutlet>>());
    pl->insert(std::make_unique<Process::PortFactory_T<Process::LineEdit>>());
    pl->insert(std::make_unique<Process::PortFactory_T<Process::ComboBox>>());
    ports = pl.get();
    compData.factories.insert(
        {Process::PortFactory::static_interfaceKey(), std::move(pl)});
  }

  const score::ApplicationContext& context() const override { return ctx; }
  const score::ApplicationComponents& components() const override { return comps; }
};

// Constructed at static-init time: every serializer constructor dereferences
// score::AppComponents(), so the mock application must exist before the
// first marshall call of the first test.
static TestApplication g_app;

Process::PortFactoryList& portFactories()
{
  return *g_app.ports;
}

// Round-trips a port through the polymorphic DataStream save/load path.
template <typename Base, typename T>
T* dsPortRoundTrip(T& port, QObject* parent)
{
  const QByteArray bytes = score::marshall<DataStream>(static_cast<const Base&>(port));
  auto loaded
      = deserialize_interface(portFactories(), DataStream::Deserializer{bytes}, parent);
  return dynamic_cast<T*>(loaded);
}

// Round-trips a port through the polymorphic JSON save/load path, going all
// the way down to UTF-8 bytes and back.
template <typename Base, typename T>
T* jsonPortRoundTrip(T& port, QObject* parent)
{
  const auto reader = score::marshall<JSONObject>(static_cast<const Base&>(port));
  const QByteArray bytes = reader.toByteArray();
  REQUIRE(!bytes.isEmpty());
  const rapidjson::Document doc = readJson(bytes);
  REQUIRE(!doc.HasParseError());
  JSONObject::Deserializer des{doc};
  auto loaded = deserialize_interface(portFactories(), des, parent);
  return dynamic_cast<T*>(loaded);
}
}

TEST_CASE("Port construction invariants", "[process][port]")
{
  QObject owner;
  auto& inlet = *new Process::ControlInlet{"in", Id<Process::Port>{7}, &owner};

  CHECK(inlet.id() == Id<Process::Port>{7});
  CHECK(inlet.type() == Process::PortType::Message);
  CHECK(inlet.parent() == &owner);
  CHECK(inlet.cables().empty());

  inlet.setName("Level");
  CHECK(inlet.name() == "Level");

  const auto addr = State::parseAddressAccessor("dev:/x@[0]");
  REQUIRE(addr.has_value());
  inlet.setAddress(*addr);
  CHECK(inlet.address() == *addr);

  // setValue stores + notifies with the new value
  ossia::value notified;
  int notifications = 0;
  QObject::connect(
      &inlet, &Process::ControlInlet::valueChanged, &owner,
      [&](const ossia::value& v) {
    notified = v;
    ++notifications;
      });
  inlet.setValue(0.75f);
  CHECK(inlet.value() == ossia::value{0.75f});
  CHECK(notified == ossia::value{0.75f});
  CHECK(notifications == 1);
  // same value again: no re-notification
  inlet.setValue(0.75f);
  CHECK(notifications == 1);

  // Port type of the other concrete classes
  Process::AudioInlet ai{"ai", Id<Process::Port>{1}, &owner};
  Process::AudioOutlet ao{"ao", Id<Process::Port>{2}, &owner};
  Process::MidiInlet mi{"mi", Id<Process::Port>{3}, &owner};
  Process::MidiOutlet mo{"mo", Id<Process::Port>{4}, &owner};
  Process::ValueInlet vi{"vi", Id<Process::Port>{5}, &owner};
  Process::ValueOutlet vo{"vo", Id<Process::Port>{6}, &owner};
  CHECK(ai.type() == Process::PortType::Audio);
  CHECK(ao.type() == Process::PortType::Audio);
  CHECK(mi.type() == Process::PortType::Midi);
  CHECK(mo.type() == Process::PortType::Midi);
  CHECK(vi.type() == Process::PortType::Message);
  CHECK(vo.type() == Process::PortType::Message);
}

TEST_CASE("ControlInlet round-trips via DataStream and JSON", "[process][port][serialization]")
{
  QObject owner;
  Process::ControlInlet port{"Gain", Id<Process::Port>{1234}, &owner};
  port.setName("Gain");
  port.setAddress(*State::parseAddressAccessor("foo:/bar@[1]"));
  port.setValue(2.5f);
  port.setInit(1.25f);
  port.setDomain(State::Domain{ossia::make_domain(0.f, 10.f)});
  port.setExposed("gain");
  port.setDescription("some control");

  SECTION("DataStream, saved through the base class")
  {
    auto ptr = dsPortRoundTrip<Process::Inlet>(port, &owner);
    REQUIRE(ptr);
    CHECK(ptr->id() == port.id());
    CHECK(ptr->name() == port.name());
    CHECK(ptr->address() == port.address());
    CHECK(ptr->value() == port.value());
    CHECK(ptr->init() == port.init());
    CHECK(ptr->domain() == port.domain());
    CHECK(ptr->exposed() == port.exposed());
    CHECK(ptr->description() == port.description());
  }

  SECTION("DataStream, saved through the concrete class")
  {
    auto ptr = dsPortRoundTrip<Process::ControlInlet>(port, &owner);
    REQUIRE(ptr);
    CHECK(ptr->id() == port.id());
    CHECK(ptr->value() == port.value());
    CHECK(ptr->domain() == port.domain());
  }

  SECTION("JSON, saved through the base class")
  {
    auto ptr = jsonPortRoundTrip<Process::Inlet>(port, &owner);
    REQUIRE(ptr);
    CHECK(ptr->id() == port.id());
    CHECK(ptr->name() == port.name());
    CHECK(ptr->address() == port.address());
    CHECK(ptr->value() == port.value());
    CHECK(ptr->domain() == port.domain());
  }
}

TEST_CASE("AudioOutlet persists gain, pan and propagate", "[process][port][serialization]")
{
  QObject owner;
  Process::AudioOutlet port{"Out", Id<Process::Port>{42}, &owner};
  port.setGain(0.7);
  port.setPan(Process::pan_weight{0.3, 0.7});
  port.setPropagate(true);
  port.setAddress(*State::parseAddressAccessor("audio:/out/main"));

  SECTION("DataStream")
  {
    auto ptr = dsPortRoundTrip<Process::Outlet>(port, &owner);
    REQUIRE(ptr);
    CHECK(ptr->id() == port.id());
    CHECK(ptr->gain() == 0.7);
    CHECK(ptr->pan() == Process::pan_weight{0.3, 0.7});
    CHECK(ptr->propagate() == true);
    CHECK(ptr->address() == port.address());
    // The implicit gain/pan child inlets must be recreated
    REQUIRE(ptr->gainInlet);
    REQUIRE(ptr->panInlet);
  }

  SECTION("JSON")
  {
    auto ptr = jsonPortRoundTrip<Process::Outlet>(port, &owner);
    REQUIRE(ptr);
    CHECK(ptr->gain() == 0.7);
    CHECK(ptr->pan() == Process::pan_weight{0.3, 0.7});
    CHECK(ptr->propagate() == true);
  }
}

TEST_CASE("ComboBox alternatives survive round-trip", "[process][port][serialization]")
{
  QObject owner;
  Process::ComboBox port{
      std::vector<std::pair<QString, ossia::value>>{{"low", 1.5f}, {"high", 4.5f}},
      4.5f, "Quality", Id<Process::Port>{99}, &owner};
  port.setAddress(*State::parseAddressAccessor("foo:/quality"));

  SECTION("DataStream")
  {
    auto ptr = dsPortRoundTrip<Process::Inlet>(port, &owner);
    REQUIRE(ptr);
    CHECK(ptr->id() == port.id());
    CHECK(ptr->value() == port.value());
    CHECK(ptr->alternatives == port.alternatives);
  }

  SECTION("JSON")
  {
    auto ptr = jsonPortRoundTrip<Process::Inlet>(port, &owner);
    REQUIRE(ptr);
    CHECK(ptr->value() == port.value());
    CHECK(ptr->alternatives == port.alternatives);
  }
}

TEST_CASE("LineEdit round-trips as its concrete type", "[process][port][serialization]")
{
  QObject owner;
  Process::LineEdit port{"initial text", "Script", Id<Process::Port>{5}, &owner};
  port.setValue(std::string{"edited text"});

  auto ds = dsPortRoundTrip<Process::Inlet>(port, &owner);
  REQUIRE(ds);
  CHECK(ds->id() == port.id());
  CHECK(ds->value() == port.value());

  auto js = jsonPortRoundTrip<Process::Inlet>(port, &owner);
  REQUIRE(js);
  CHECK(js->value() == port.value());
}

TEST_CASE("ControlInlet saveData/loadData round-trip", "[process][port]")
{
  QObject owner;
  Process::ControlInlet a{"a", Id<Process::Port>{1}, &owner};
  a.setAddress(*State::parseAddressAccessor("dev:/ctl"));
  a.setValue(3.5f);
  a.setDomain(State::Domain{ossia::make_domain(0.f, 5.f)});

  // The address always round-trips (saved by Port::saveData layer).
  Process::ControlInlet b{"b", Id<Process::Port>{2}, &owner};
  b.loadData(a.saveData());
  CHECK(b.address() == a.address());
  // loadData must not clobber identity
  CHECK(b.id() == Id<Process::Port>{2});

  // The default (NoFlag) deliberately does NOT restore the value; passing
  // DontReloadValue is what restores it. The flag is misnamed relative to its
  // effect, but this is by design, NOT a data-loss bug (full analysis in the
  // P3R2 report): the only caller that wants the value back —
  // LoadPresetCommand::redo — passes the flag; every default-flag caller
  // (script/avnd port rebuild, LV2 EffectModel) intentionally re-derives the
  // value from its own source of truth (script defaults / lilv state /
  // loadPreset). Document save/load does NOT use loadData at all — it goes
  // through the value visitor which preserves m_value unconditionally. So a
  // naive loadData(saveData()) "dropping" the value is expected and harmless.
  // The right cleanup is a RENAME (DontReloadValue -> ReloadValue), not a
  // polarity flip (a flip would break every call site).
  CHECK(b.value() != a.value()); // default flags: value NOT restored (by design)

  Process::ControlInlet c{"c", Id<Process::Port>{3}, &owner};
  c.loadData(a.saveData(), Process::PortLoadDataFlags::DontReloadValue);
  CHECK(c.value() == a.value()); // "DontReloadValue" DOES restore it

  // Note: the domain is not part of saveData() at all (only cables, address,
  // value) - by design, so no expectation on it here.
}

TEST_CASE("Corrupt port buffers load to nullptr without crashing", "[process][port][fuzz]")
{
  QObject owner;
  Process::ControlInlet port{"c", Id<Process::Port>{1}, &owner};
  port.setValue(0.5f);
  const QByteArray full
      = score::marshall<DataStream>(static_cast<const Process::Inlet&>(port));

  // Truncations: the interface loader must catch framing errors.
  for(int len = 0; len < full.size(); len += 3)
  {
    // A prefix long enough to contain the whole nested object may load;
    // anything shorter must come back null. Loaded ports are owned by
    // `owner` through QObject parenting.
    (void)deserialize_interface(
        portFactories(), DataStream::Deserializer{full.left(len)}, &owner);
  }

  // Unknown factory uuid: flip bytes in the key region -> loadMissing -> null.
  QByteArray bad = full;
  for(int i = 4; i < std::min(20, (int)bad.size()); ++i)
    bad[i] = static_cast<char>(~bad[i]);
  auto loaded
      = deserialize_interface(portFactories(), DataStream::Deserializer{bad}, &owner);
  CHECK(loaded == nullptr);
}

//////////////////////////////////////////////////////////////////////////////
// Cables
//////////////////////////////////////////////////////////////////////////////

namespace
{
// NOTE: Process::CableData declares a friend operator== (CableData.hpp:23)
// but no definition exists anywhere in the codebase, so using it fails to
// link (undefined symbol). Field-wise comparison instead; see the P3R2
// report for the bug entry.
bool sameCableData(const Process::CableData& lhs, const Process::CableData& rhs)
{
  return lhs.type == rhs.type && lhs.source == rhs.source && lhs.sink == rhs.sink;
}

Path<Process::Outlet> makeOutletPath(int process, int port)
{
  return Path<Process::Outlet>{
      ObjectPath{
          {"Scenario::ProcessModel", 0},
          {"TestProcess", process},
          {"Port", port}},
      Path<Process::Outlet>::UnsafeDynamicCreation{}};
}
Path<Process::Inlet> makeInletPath(int process, int port)
{
  return Path<Process::Inlet>{
      ObjectPath{
          {"Scenario::ProcessModel", 0},
          {"TestProcess", process},
          {"Port", port}},
      Path<Process::Inlet>::UnsafeDynamicCreation{}};
}
}

TEST_CASE("Cable stores and round-trips its endpoints", "[process][cable][serialization]")
{
  QObject owner;

  Process::CableData data;
  data.type = Process::CableType::ImmediateStrict;
  data.source = makeOutletPath(1, 10);
  data.sink = makeInletPath(2, 20);

  Process::Cable cable{Id<Process::Cable>{77}, data, &owner};

  SECTION("construction reflects the data")
  {
    CHECK(cable.id() == Id<Process::Cable>{77});
    CHECK(cable.type() == Process::CableType::ImmediateStrict);
    CHECK(cable.source() == Path<Process::Outlet>{data.source.unsafePath(), Path<Process::Outlet>::UnsafeDynamicCreation{}});
    CHECK(cable.sink() == Path<Process::Inlet>{data.sink.unsafePath(), Path<Process::Inlet>::UnsafeDynamicCreation{}});
    CHECK(sameCableData(cable.toCableData(), data));
  }

  SECTION("update replaces endpoints")
  {
    Process::CableData other;
    other.type = Process::CableType::DelayedGlutton;
    other.source = makeOutletPath(3, 30);
    other.sink = makeInletPath(4, 40);
    cable.update(other);
    CHECK(sameCableData(cable.toCableData(), other));
    CHECK(cable.type() == Process::CableType::DelayedGlutton);
  }

  SECTION("DataStream round-trip")
  {
    const QByteArray bytes = score::marshall<DataStream>(cable);
    DataStream::Deserializer des{bytes};
    Process::Cable loaded{des, &owner};
    CHECK(loaded.id() == cable.id());
    CHECK(loaded.type() == cable.type());
    CHECK(sameCableData(loaded.toCableData(), cable.toCableData()));
  }

  SECTION("JSON round-trip through bytes")
  {
    const QByteArray bytes = toJson(cable);
    const rapidjson::Document doc = readJson(bytes);
    REQUIRE(!doc.HasParseError());
    JSONObject::Deserializer des{doc};
    Process::Cable loaded{des, &owner};
    CHECK(loaded.id() == cable.id());
    CHECK(loaded.type() == cable.type());
    CHECK(sameCableData(loaded.toCableData(), cable.toCableData()));
  }

  SECTION("CableData value round-trip")
  {
    const auto ds = score::unmarshall<Process::CableData>(score::marshall<DataStream>(data));
    CHECK(sameCableData(ds, data));

    const auto js = fromJson<Process::CableData>(toJson(data));
    CHECK(sameCableData(js, data));
  }
}

//////////////////////////////////////////////////////////////////////////////
// A minimal concrete process: object graph + serialization
//////////////////////////////////////////////////////////////////////////////

namespace
{
class TestProcess final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
public:
  using Process::ProcessModel::m_inlets;
  using Process::ProcessModel::m_outlets;

  TestProcess(TimeVal duration, const Id<Process::ProcessModel>& id, QObject* parent)
      : Process::ProcessModel{duration, id, "TestProcess", parent}
  {
    auto in = new Process::ControlInlet{"Control", Id<Process::Port>{0}, this};
    in->setName("Control");
    in->setValue(0.5f);
    m_inlets.push_back(in);

    auto audioIn = new Process::AudioInlet{"AudioIn", Id<Process::Port>{1}, this};
    m_inlets.push_back(audioIn);

    auto out = new Process::AudioOutlet{"AudioOut", Id<Process::Port>{0}, this};
    out->setPropagate(true);
    m_outlets.push_back(out);
  }

  template <typename Impl>
  TestProcess(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~TestProcess() override
  {
    // Owned through the QObject hierarchy; nothing else to do.
  }

  static UuidKey<Process::ProcessModel> static_concreteKey() noexcept
  {
    return UuidKey<Process::ProcessModel>{"5d1b9a1e-2f43-4c34-9d61-2b0e40f92a11"};
  }
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return static_concreteKey();
  }
  void serialize_impl(const VisitorVariant& vis) const override
  {
    score::serialize_dyn(vis, *this);
  }

  QString prettyShortName() const noexcept override { return "Test"; }
  QString category() const noexcept override { return "Test"; }
  QStringList tags() const noexcept override { return {}; }
  Process::ProcessFlags flags() const noexcept override
  {
    return Process::ProcessFlags::SupportsAll;
  }
};
}

// Serialization of the derived part: exactly the shape real processes use.
template <>
void DataStreamReader::read(const TestProcess& proc)
{
  Process::readPorts(*this, proc.m_inlets, proc.m_outlets);
  insertDelimiter();
}
template <>
void DataStreamWriter::write(TestProcess& proc)
{
  Process::writePorts(*this, portFactories(), proc.m_inlets, proc.m_outlets, &proc);
  checkDelimiter();
}
template <>
void JSONReader::read(const TestProcess& proc)
{
  Process::readPorts(*this, proc.m_inlets, proc.m_outlets);
}
template <>
void JSONWriter::write(TestProcess& proc)
{
  Process::writePorts(*this, portFactories(), proc.m_inlets, proc.m_outlets, &proc);
}

TEST_CASE("Concrete process: object graph invariants", "[process][model]")
{
  QObject owner;
  TestProcess proc{TimeVal::fromMsecs(5000), Id<Process::ProcessModel>{1}, &owner};

  CHECK(proc.duration() == TimeVal::fromMsecs(5000));
  REQUIRE(proc.inlets().size() == 2);
  REQUIRE(proc.outlets().size() == 1);

  // Port lookup by id resolves to the exact same object
  CHECK(proc.inlet(Id<Process::Port>{0}) == proc.inlets()[0]);
  CHECK(proc.inlet(Id<Process::Port>{1}) == proc.inlets()[1]);
  CHECK(proc.outlet(Id<Process::Port>{0}) == proc.outlets()[0]);
  CHECK(proc.inlet(Id<Process::Port>{123}) == nullptr);

  // Ports are parented to the process (ownership + path resolution)
  for(auto* p : proc.inlets())
    CHECK(p->parent() == &proc);
  for(auto* p : proc.outlets())
    CHECK(p->parent() == &proc);

  // Duration setters
  proc.setDuration(TimeVal::fromMsecs(1000));
  CHECK(proc.duration() == TimeVal::fromMsecs(1000));
}

namespace
{
void checkProcessEquality(const TestProcess& loaded, const TestProcess& proc)
{
  CHECK(loaded.id() == proc.id());
  CHECK(loaded.duration() == proc.duration());
  CHECK(loaded.position() == proc.position());
  CHECK(loaded.size() == proc.size());
  CHECK(loaded.loops() == proc.loops());
  CHECK(loaded.startOffset() == proc.startOffset());
  CHECK(loaded.metadata().getName() == proc.metadata().getName());

  REQUIRE(loaded.inlets().size() == proc.inlets().size());
  REQUIRE(loaded.outlets().size() == proc.outlets().size());
  for(std::size_t i = 0; i < proc.inlets().size(); i++)
  {
    CHECK(loaded.inlets()[i]->id() == proc.inlets()[i]->id());
    CHECK(loaded.inlets()[i]->type() == proc.inlets()[i]->type());
    CHECK(loaded.inlets()[i]->parent() == &loaded);
  }
  auto loadedControl = dynamic_cast<Process::ControlInlet*>(loaded.inlets()[0]);
  auto origControl = dynamic_cast<Process::ControlInlet*>(proc.inlets()[0]);
  REQUIRE(loadedControl);
  REQUIRE(origControl);
  CHECK(loadedControl->value() == origControl->value());
  CHECK(loadedControl->name() == origControl->name());

  auto loadedOut = dynamic_cast<Process::AudioOutlet*>(loaded.outlets()[0]);
  REQUIRE(loadedOut);
  CHECK(loadedOut->propagate() == true);
}
}

TEST_CASE("Concrete process: DataStream round-trip", "[process][model][serialization]")
{
  QObject owner;
  TestProcess proc{TimeVal::fromMsecs(3000), Id<Process::ProcessModel>{4}, &owner};
  proc.setPosition({10., 20.});
  proc.setSize({300., 150.});
  proc.setLoops(true);
  proc.setStartOffset(TimeVal::fromMsecs(250));

  // Serialize through the base class, as documents do.
  const QByteArray bytes
      = score::marshall<DataStream>(static_cast<const Process::ProcessModel&>(proc));

  // Mimic deserialize_interface: unwrap the nested buffer, check the concrete
  // key, then run the concrete deserializing constructor.
  DataStream::Deserializer des{bytes};
  QByteArray nested;
  des.stream() >> nested;
  DataStream::Deserializer sub{nested};

  SCORE_DEBUG_CHECK_DELIMITER2(sub);
  UuidKey<Process::ProcessModel> key;
  TSerializer<DataStream, UuidKey<Process::ProcessModel>>::writeTo(sub, key);
  SCORE_DEBUG_CHECK_DELIMITER2(sub);
  REQUIRE(key == TestProcess::static_concreteKey());

  TestProcess loaded{sub, &owner};
  checkProcessEquality(loaded, proc);
}

TEST_CASE("Concrete process: JSON round-trip", "[process][model][serialization]")
{
  QObject owner;
  TestProcess proc{TimeVal::fromMsecs(3000), Id<Process::ProcessModel>{4}, &owner};
  proc.setPosition({10., 20.});
  proc.setSize({300., 150.});
  proc.setLoops(true);

  const auto reader
      = score::marshall<JSONObject>(static_cast<const Process::ProcessModel&>(proc));
  const QByteArray bytes = reader.toByteArray();
  const rapidjson::Document doc = readJson(bytes);
  REQUIRE(!doc.HasParseError());

  // JSON is keyed: the concrete constructor can read straight from the object.
  JSONObject::Deserializer des{doc};
  TestProcess loaded{des, &owner};
  checkProcessEquality(loaded, proc);
}

TEST_CASE("Ports vector round-trip via readPorts/writePorts", "[process][port][serialization]")
{
  QObject owner;
  Process::Inlets ins;
  Process::Outlets outs;
  {
    auto c = new Process::ControlInlet{"c0", Id<Process::Port>{0}, &owner};
    c->setValue(1.5f);
    ins.push_back(c);
    ins.push_back(new Process::MidiInlet{"m1", Id<Process::Port>{1}, &owner});
    auto ao = new Process::AudioOutlet{"a0", Id<Process::Port>{0}, &owner};
    ao->setGain(0.25);
    outs.push_back(ao);
    outs.push_back(new Process::ControlOutlet{"c1", Id<Process::Port>{1}, &owner});
  }

  QByteArray bytes;
  {
    DataStreamReader r{&bytes};
    Process::readPorts(r, ins, outs);
  }

  QObject owner2;
  Process::Inlets ins2;
  Process::Outlets outs2;
  {
    DataStreamWriter w{bytes};
    Process::writePorts(w, portFactories(), ins2, outs2, &owner2);
  }

  REQUIRE(ins2.size() == 2);
  REQUIRE(outs2.size() == 2);
  CHECK(ins2[0]->id() == ins[0]->id());
  CHECK(ins2[1]->id() == ins[1]->id());
  CHECK(outs2[0]->id() == outs[0]->id());
  CHECK(outs2[1]->id() == outs[1]->id());
  CHECK(ins2[1]->type() == Process::PortType::Midi);

  auto c2 = dynamic_cast<Process::ControlInlet*>(ins2[0]);
  REQUIRE(c2);
  CHECK(c2->value() == ossia::value{1.5f});

  auto ao2 = dynamic_cast<Process::AudioOutlet*>(outs2[0]);
  REQUIRE(ao2);
  CHECK(ao2->gain() == 0.25);
}
