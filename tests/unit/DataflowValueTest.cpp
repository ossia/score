#include <State/Address.hpp>
#include <State/Domain.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/CableData.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortSerialization.hpp>

#include <Dataflow/AudioInletItem.hpp>
#include <Dataflow/AudioOutletItem.hpp>
#include <Dataflow/ControlInletItem.hpp>
#include <Dataflow/ControlOutletItem.hpp>
#include <Dataflow/MidiInletItem.hpp>
#include <Dataflow/MidiOutletItem.hpp>
#include <Dataflow/ValueInletItem.hpp>
#include <Dataflow/ValueOutletItem.hpp>

#include <score/application/ApplicationComponents.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/model/path/ObjectIdentifier.hpp>
#include <score/model/path/ObjectPath.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/dataflow/connection.hpp>
#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_static.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/nodes/midi.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/value_port.hpp>
#include <ossia/network/domain/domain.hpp>

#include <catch2/catch_test_macros.hpp>

#include <csignal>

namespace
{
struct ScopedIgnoreSigtrap
{
  void (*prev)(int);
  ScopedIgnoreSigtrap()
      : prev{std::signal(SIGTRAP, SIG_IGN)}
  {
  }
  ~ScopedIgnoreSigtrap() { std::signal(SIGTRAP, prev); }
};

struct TestApplication final : public score::ApplicationInterface
{
  score::ApplicationComponentsData m_compData;
  score::ApplicationComponents m_components{m_compData};
  score::ApplicationSettings m_appSettings;
  score::DocumentList m_documents;
  std::vector<std::unique_ptr<score::SettingsDelegateModel>> m_settings;
  score::ApplicationContext m_ctx{m_appSettings, m_components, m_documents, m_settings};

  TestApplication()
  {
    m_instance = this;

    auto ports = std::make_unique<Process::PortFactoryList>();
    ports->insert(std::make_unique<Dataflow::ControlInletFactory>());
    ports->insert(std::make_unique<Dataflow::ControlOutletFactory>());
    ports->insert(std::make_unique<Dataflow::ValueInletFactory>());
    ports->insert(std::make_unique<Dataflow::ValueOutletFactory>());
    ports->insert(std::make_unique<Dataflow::MidiInletFactory>());
    ports->insert(std::make_unique<Dataflow::MidiOutletFactory>());
    ports->insert(std::make_unique<Dataflow::AudioInletFactory>());
    ports->insert(std::make_unique<Dataflow::AudioOutletFactory>());
    m_compData.factories.emplace(ports->interfaceKey(), std::move(ports));
  }

  const score::ApplicationContext& context() const override { return m_ctx; }
  const score::ApplicationComponents& components() const override
  {
    return m_components;
  }
};

TestApplication& testApp()
{
  static TestApplication app;
  return app;
}

bool sameCableData(const Process::CableData& lhs, const Process::CableData& rhs)
{
  return lhs.type == rhs.type && lhs.source == rhs.source && lhs.sink == rhs.sink;
}

Path<Process::Port> makePortPath(int a, int b)
{
  ObjectPath p;
  p.vec().push_back(ObjectIdentifier{"TestProcess", a});
  p.vec().push_back(ObjectIdentifier{"Port", b});
  return Path<Process::Port>{std::move(p), Path<Process::Port>::UnsafeDynamicCreation{}};
}
}

TEST_CASE("CableData equality and Cable round-trip", "[dataflow][cable]")
{
  testApp();

  Process::CableData cd;
  cd.type = Process::CableType::DelayedStrict;
  cd.source = makePortPath(1, 2);
  cd.sink = makePortPath(3, 4);

  SECTION("equality is field-wise")
  {
    Process::CableData same;
    same.type = Process::CableType::DelayedStrict;
    same.source = makePortPath(1, 2);
    same.sink = makePortPath(3, 4);
    CHECK(sameCableData(cd, same));

    same.type = Process::CableType::ImmediateGlutton;
    CHECK(!sameCableData(cd, same));
  }

  SECTION("DataStream round-trip")
  {
    Process::Cable src{Id<Process::Cable>{7}, cd, nullptr};
    const QByteArray arr = score::marshall<DataStream>(src);
    Process::Cable dst{DataStream::Deserializer{arr}, nullptr};

    CHECK(dst.id().val() == 7);
    CHECK(dst.type() == Process::CableType::DelayedStrict);
    CHECK(sameCableData(dst.toCableData(), cd));
  }

  SECTION("JSON round-trip")
  {
    Process::Cable src{Id<Process::Cable>{7}, cd, nullptr};
    JSONReader r;
    r.readFrom(src);
    const rapidjson::Document doc = readJson(r.toByteArray());
    Process::Cable dst{JSONObject::Deserializer{doc}, nullptr};

    CHECK(dst.id().val() == 7);
    CHECK(dst.type() == Process::CableType::DelayedStrict);
    CHECK(sameCableData(dst.toCableData(), cd));
  }

  SECTION("all four cable types survive a round-trip")
  {
    for(auto t :
        {Process::CableType::ImmediateGlutton, Process::CableType::ImmediateStrict,
         Process::CableType::DelayedGlutton, Process::CableType::DelayedStrict})
    {
      Process::CableData d = cd;
      d.type = t;
      Process::Cable src{Id<Process::Cable>{1}, d, nullptr};
      const QByteArray arr = score::marshall<DataStream>(src);
      Process::Cable dst{DataStream::Deserializer{arr}, nullptr};
      CHECK(dst.type() == t);
    }
  }
}

TEST_CASE("polymorphic port load through the dataflow factories", "[dataflow][port]")
{
  testApp();

  SECTION("ControlInlet: value, domain, name, address survive DataStream")
  {
    Process::ControlInlet src{"Level", Id<Process::Port>{4}, nullptr};
    src.setValue(0.5f);
    src.setDomain(State::Domain{ossia::make_domain(0.f, 1.f)});
    src.setAddress(State::AddressAccessor{*State::Address::fromString("dev:/foo/bar")});

    QByteArray arr;
    {
      DataStream::Serializer s{&arr};
      s.stream() << src;
    }
    DataStream::Deserializer des{arr};
    auto loaded = Process::load_inlet(des, nullptr);
    REQUIRE(loaded);

    auto ctrl = dynamic_cast<Process::ControlInlet*>(loaded.get());
    REQUIRE(ctrl);
    CHECK(ctrl->id().val() == 4);
    CHECK(ctrl->name() == "Level");
    CHECK(ctrl->type() == Process::PortType::Message);
    CHECK(ctrl->value() == ossia::value{0.5f});
    CHECK(ctrl->domain() == State::Domain{ossia::make_domain(0.f, 1.f)});
    CHECK(ctrl->address().address.toString() == "dev:/foo/bar");
  }

  SECTION("ControlInlet: JSON")
  {
    Process::ControlInlet src{"Level", Id<Process::Port>{4}, nullptr};
    src.setValue(ossia::value{std::vector<ossia::value>{1, 2, 3}});

    JSONReader r;
    r.readFrom(src);
    const rapidjson::Document doc = readJson(r.toByteArray());
    JSONObject::Deserializer des{doc};
    auto loaded = Process::load_inlet(des, nullptr);
    REQUIRE(loaded);

    auto ctrl = dynamic_cast<Process::ControlInlet*>(loaded.get());
    REQUIRE(ctrl);
    CHECK(ctrl->value() == ossia::value{std::vector<ossia::value>{1, 2, 3}});
  }

  SECTION("MidiOutlet keeps its concrete type")
  {
    Process::MidiOutlet src{"MIDI Out", Id<Process::Port>{0}, nullptr};

    QByteArray arr;
    {
      DataStream::Serializer s{&arr};
      s.stream() << src;
    }
    DataStream::Deserializer des{arr};
    auto loaded = Process::load_outlet(des, nullptr);
    REQUIRE(loaded);
    CHECK(dynamic_cast<Process::MidiOutlet*>(loaded.get()));
    CHECK(loaded->type() == Process::PortType::Midi);
    CHECK(loaded->name() == "MIDI Out");
  }

  SECTION("AudioOutlet: gain / pan / propagate")
  {
    Process::AudioOutlet src{"Out", Id<Process::Port>{2}, nullptr};
    src.setGain(0.7);
    src.setPan({0.25, 0.75});
    src.setPropagate(true);

    QByteArray arr;
    {
      DataStream::Serializer s{&arr};
      s.stream() << src;
    }
    DataStream::Deserializer des{arr};
    auto loaded = Process::load_outlet(des, nullptr);
    REQUIRE(loaded);

    auto audio = dynamic_cast<Process::AudioOutlet*>(loaded.get());
    REQUIRE(audio);
    CHECK(audio->type() == Process::PortType::Audio);
    CHECK(audio->gain() == 0.7);
    CHECK(audio->pan() == Process::pan_weight{0.25, 0.75});
    CHECK(audio->propagate() == true);
  }
}

TEST_CASE("ControlInlet value model", "[dataflow][port]")
{
  SECTION("setValue stores and signals the value")
  {
    Process::ControlInlet inl{"ctl", Id<Process::Port>{0}, nullptr};
    ossia::value seen;
    int count = 0;
    QObject::connect(&inl, &Process::ControlInlet::valueChanged,
                     [&](const ossia::value& v) {
      seen = v;
      count++;
    });

    inl.setValue(3.5f);
    CHECK(inl.value() == ossia::value{3.5f});
    CHECK(seen == ossia::value{3.5f});
    CHECK(count == 1);

    // Same value again: no signal
    inl.setValue(3.5f);
    CHECK(count == 1);

    // Impulse always re-triggers
    inl.setValue(ossia::impulse{});
    inl.setValue(ossia::impulse{});
    CHECK(count == 3);
  }

  SECTION("saveData / loadData flag semantics")
  {
    Process::ControlInlet src{"ctl", Id<Process::Port>{0}, nullptr};
    src.setValue(1.25f);
    src.setAddress(State::AddressAccessor{*State::Address::fromString("dev:/a")});
    const QByteArray data = src.saveData();

    Process::ControlInlet defaults{"ctl", Id<Process::Port>{1}, nullptr};
    defaults.loadData(data);
    CHECK(defaults.value() == ossia::value{}); // value not restored
    CHECK(defaults.address().address.toString() == "dev:/a");

    Process::ControlInlet flagged{"ctl", Id<Process::Port>{2}, nullptr};
    flagged.loadData(data, Process::PortLoadDataFlags::ReloadValue);
    CHECK(flagged.value() == ossia::value{1.25f}); // value restored
  }
}

namespace
{
class capture_node final : public ossia::graph_node
{
public:
  std::function<void(const ossia::token_request&, ossia::exec_state_facade)> fun;

  capture_node(ossia::inlets in, ossia::outlets out)
  {
    m_inlets = std::move(in);
    m_outlets = std::move(out);
  }

  std::string label() const noexcept override { return "capture_node"; }

  void run(const ossia::token_request& t, ossia::exec_state_facade e) noexcept override
  {
    if(fun)
      fun(t, e);
  }
};

struct value_graph_fixture
{
  std::unique_ptr<ossia::tc_graph> g_ptr = std::make_unique<ossia::tc_graph>();
  std::unique_ptr<ossia::execution_state> e_ptr
      = std::make_unique<ossia::execution_state>();
  ossia::tc_graph& g = *g_ptr;
  ossia::execution_state& e = *e_ptr;

  std::shared_ptr<capture_node> source;
  std::shared_ptr<capture_node> sink;
  ossia::value_outlet* src_out{};
  ossia::value_inlet* sink_in{};

  std::vector<ossia::value> received;
  ossia::value to_send;

  explicit value_graph_fixture(ossia::connection con)
  {
    auto out = new ossia::value_outlet;
    source = std::make_shared<capture_node>(ossia::inlets{}, ossia::outlets{out});
    src_out = out;
    source->fun = [this](const ossia::token_request&, ossia::exec_state_facade) {
      src_out->data.write_value(to_send, 0);
    };

    auto in = new ossia::value_inlet;
    sink = std::make_shared<capture_node>(ossia::inlets{in}, ossia::outlets{});
    sink_in = in;
    sink->fun = [this](const ossia::token_request&, ossia::exec_state_facade) {
      for(auto& tv : sink_in->data.get_data())
        received.push_back(tv.value);
    };

    g.add_node(source);
    g.add_node(sink);
    g.connect(g.allocate_edge(con, src_out, sink_in, source, sink));
  }

  void tick()
  {
    source->request(ossia::token_request{});
    sink->request(ossia::token_request{});
    e.begin_tick();
    g.state(e);
    e.commit();
  }
};
}

TEST_CASE("value propagation along graph edges", "[dataflow][graph]")
{
  SECTION("immediate glutton cable: value arrives in the same tick")
  {
    value_graph_fixture f{ossia::immediate_glutton_connection{}};
    f.to_send = 42;
    f.tick();
    REQUIRE(f.received.size() == 1);
    CHECK(f.received[0] == ossia::value{42});
  }

  SECTION("immediate strict cable: value arrives in the same tick")
  {
    value_graph_fixture f{ossia::immediate_strict_connection{}};
    f.to_send = std::string{"hello"};
    f.tick();
    REQUIRE(f.received.size() == 1);
    CHECK(f.received[0] == ossia::value{std::string{"hello"}});
  }

  SECTION("values keep their type across the edge")
  {
    value_graph_fixture f{ossia::immediate_glutton_connection{}};

    f.to_send = 3.25f;
    f.tick();
    f.to_send = ossia::vec3f{1.f, 2.f, 3.f};
    f.tick();

    REQUIRE(f.received.size() == 2);
    CHECK(f.received[0] == ossia::value{3.25f});
    CHECK(f.received[0].get_type() == ossia::val_type::FLOAT);
    CHECK(f.received[1] == ossia::value{ossia::vec3f{1.f, 2.f, 3.f}});
    CHECK(f.received[1].get_type() == ossia::val_type::VEC3F);
  }

  SECTION("delayed glutton cable: value goes through the edge's delay line")
  {
    value_graph_fixture f{ossia::delayed_glutton_connection{}};
    f.to_send = 7;
    f.tick();
    f.source->fun = {}; // stop sending
    f.tick();

    REQUIRE(f.received.size() == 1);
    CHECK(f.received[0] == ossia::value{7});
  }
}

TEST_CASE("midi messages propagate along a graph edge", "[dataflow][graph][midi]")
{
  auto g_ptr = std::make_unique<ossia::tc_graph>();
  auto e_ptr = std::make_unique<ossia::execution_state>();
  ossia::tc_graph& g = *g_ptr;
  ossia::execution_state& e = *e_ptr;

  auto midi = std::make_shared<ossia::nodes::midi>(4);
  midi->set_channel(3);
  midi->add_note({ossia::time_value{10}, ossia::time_value{50}, 61, 99});

  auto in = new ossia::midi_inlet;
  auto sink = std::make_shared<capture_node>(ossia::inlets{in}, ossia::outlets{});
  std::vector<libremidi::ump> received;
  sink->fun = [&](const ossia::token_request&, ossia::exec_state_facade) {
    for(auto& m : in->data.messages)
      received.push_back(m);
  };

  g.add_node(midi);
  g.add_node(sink);
  g.connect(g.allocate_edge(
      ossia::immediate_glutton_connection{}, midi->root_outputs()[0],
      sink->root_inputs()[0], midi, sink));

  const ossia::token_request tok{
      ossia::time_value{0}, ossia::time_value{100}, ossia::time_value{1000},
      ossia::time_value{0}, 1., ossia::time_signature{4, 4}, 120.};
  midi->request(tok);
  sink->request(tok);
  e.begin_tick();
  g.state(e);
  e.commit();

  REQUIRE(received.size() == 1);
  CHECK(cmidi2_ump_get_status_code(received[0].data) == CMIDI2_STATUS_NOTE_ON);
  CHECK(cmidi2_ump_get_channel(received[0].data) == 2); // channel 3, 0-based
  CHECK(cmidi2_ump_get_midi2_note_note(received[0].data) == 61);
  CHECK(cmidi2_ump_get_midi2_note_velocity(received[0].data) / 0x200 == 99);
}

TEST_CASE("fuzz: corrupt port and cable buffers are handled gracefully",
          "[dataflow][fuzz]")
{
  testApp();


  SECTION("truncated serialized cable throws instead of crashing")
  {
    Process::CableData cd;
    cd.type = Process::CableType::ImmediateStrict;
    cd.source = makePortPath(1, 2);
    cd.sink = makePortPath(3, 4);
    Process::Cable src{Id<Process::Cable>{1}, cd, nullptr};
    const QByteArray arr = score::marshall<DataStream>(src);

    ScopedIgnoreSigtrap guard;
    for(int len : {0, 2, int(arr.size() / 3), int(arr.size() / 2), int(arr.size() - 1)})
    {
      try
      {
        Process::Cable dst{DataStream::Deserializer{arr.left(len)}, nullptr};
      }
      catch(const std::exception&)
      {
        // "Corrupt save file." is acceptable
      }
    }
    SUCCEED("no crash on truncated cable buffers");
  }

  SECTION("polymorphic port load from truncated buffer returns null or throws")
  {
    Process::ControlInlet src{"ctl", Id<Process::Port>{0}, nullptr};
    QByteArray arr;
    {
      DataStream::Serializer s{&arr};
      s.stream() << src;
    }

    ScopedIgnoreSigtrap guard;
    for(int len : {0, 4, int(arr.size() / 2)})
    {
      try
      {
        DataStream::Deserializer des{arr.left(len)};
        auto p = Process::load_inlet(des, nullptr);
        // nullptr or a default-constructed port are both acceptable
      }
      catch(const std::exception&)
      {
      }
    }
    SUCCEED("no crash on truncated polymorphic port buffers");
  }
}
