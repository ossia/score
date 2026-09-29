// A document saved when a control of an avendish process was another kind of
// control (here LFO v3's Period, once a float slider, now a time chooser),
// loaded with the current object, in JSON and in the binary format: the
// control comes back as the declared kind with its value (as seconds), its
// address, and its id -- which the document's cables are restored against.

#include <State/Address.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Dataflow/Commands/EditConnection.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/document/Document.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <Avnd/Factories.hpp>
#include <Crousti/Executor.hpp>
#include <Crousti/ProcessModel.hpp>
#include <Fx/LFO_v3.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

namespace
{
const QString lfo_uuid = QStringLiteral("0b1b1816-c33e-4796-a16d-5aab27fe600f");

using Lfo = Nodes::LFO::v3::Node;

//! LFO v3 as it was saved when its Period was a float slider: the same uuid
//! and the same ports, but for that one, and without Lock to bars.
struct OldLfo
{
  halp_meta(name, "LFO")
  halp_meta(c_name, "LFO")
  halp_meta(category, "Control/Generators")
  halp_meta(author, "ossia score")
  halp_meta(uuid, "0b1b1816-c33e-4796-a16d-5aab27fe600f");

  struct
  {
    halp::hslider_f32<"Period", halp::range{0.01, 60., 1.}> period;
    decltype(Lfo::ins::shape) shape;
    decltype(Lfo::ins::retrigger) retrigger;
    decltype(Lfo::ins::ampl) ampl;
    decltype(Lfo::ins::offset) offset;
    decltype(Lfo::ins::jitter) jitter;
    decltype(Lfo::ins::phase) phase;
    decltype(Lfo::ins::waveform) waveform;
  } inputs;
  decltype(Lfo::outputs) outputs;

  using tick = halp::tick_flicks;
  void operator()(const tick&) { }
};

Process::ProcessModel* roundtrip_datastream(
    const Process::ProcessModel& proc, const score::DocumentContext& dctx,
    QObject* parent)
{
  auto& pl = dctx.app.interfaces<Process::ProcessFactoryList>();
  const QByteArray bytes = DataStreamReader::marshall(proc);
  DataStream::Deserializer des{bytes};
  return deserialize_interface(pl, des, dctx, parent);
}

Process::ProcessModel* roundtrip_json(
    const Process::ProcessModel& proc, const score::DocumentContext& dctx,
    QObject* parent)
{
  auto& pl = dctx.app.interfaces<Process::ProcessFactoryList>();
  JSONReader reader;
  reader.readFrom(proc);
  const auto doc = readJson(reader.toByteArray());
  JSONObject::Deserializer des{doc};
  return deserialize_interface(pl, des, dctx, parent);
}
}

TEST_CASE("A float control that became a time chooser keeps its value", "[avnd][upgrade]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto& itv = score::test::base_interval(*doc);

    // The current object, as the source of a cable into the old one
    auto* source = score::test::add_process(*doc, lfo_uuid, {});
    if(!source)
      SKIP("LFO v3 is not built");

    auto* old = new oscr::ProcessModel<OldLfo>{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{7777}, dctx, &itv};
    itv.processes.add(old);

    auto* period = qobject_cast<Process::FloatSlider*>(old->inlets()[0]);
    REQUIRE(period);
    period->setValue(2.5f);
    const State::AddressAccessor address{State::Address{"dev", {"lfo", "period"}}};
    period->setAddress(address);
    auto* shape = qobject_cast<Process::ControlInlet*>(old->inlets()[1]);
    REQUIRE(shape);
    shape->setValue(0.8f);

    auto& model = score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc);
    CommandDispatcher<>{dctx.commandStack}.submit<Dataflow::CreateCable>(
        model, Id<Process::Cable>{1}, Process::CableType::ImmediateGlutton,
        *source->outlets()[0], *period);
    REQUIRE(period->cables().size() == 1);
    const auto period_id = period->id();
    // The cable is the document's: on load it finds its sink by that id
    auto& cable = model.cables.at(Id<Process::Cable>{1});
    REQUIRE(!cable.sink().unsafePath().vec().empty());
    CHECK(cable.sink().unsafePath().vec().back().id() == period_id.val());

    for(auto roundtrip : {&roundtrip_json, &roundtrip_datastream})
    {
      auto* loaded = roundtrip(*old, dctx, &doc->model());
      REQUIRE(loaded);
      REQUIRE(loaded->inlets().size() == old->inlets().size() + 1);

      auto* chooser = qobject_cast<Process::TimeChooser*>(loaded->inlets()[0]);
      REQUIRE(chooser);
      CHECK(chooser->id() == period_id);
      // Seconds, free: what the slider meant
      CHECK(chooser->value() == ossia::value{ossia::vec2f{2.5f, 0.f}});
      CHECK(chooser->address() == address);

      // The controls that did not change are untouched
      auto* loaded_shape = qobject_cast<Process::ControlInlet*>(loaded->inlets()[1]);
      REQUIRE(loaded_shape);
      CHECK(ossia::convert<float>(loaded_shape->value()) == 0.8f);

      // A synced LFO saved before the lock counts its cycles from the start,
      // as it did then.
      auto* lock = qobject_cast<Process::ControlInlet*>(loaded->inlets().back());
      REQUIRE(lock);
      CHECK(lock->name() == QStringLiteral("Lock to bars"));
      CHECK(ossia::convert<bool>(lock->value()) == false);
      delete loaded;
    }
  });
}

namespace
{
//! A rate in Hz, as documents of this object saved it...
struct RateSpec
{
  halp_meta(name, "Rate to period")
  halp_meta(c_name, "rate_to_period_test")
  halp_meta(uuid, "5a4e8f0c-7d0b-4a51-9d7e-6c1f4b2e9a13");
  struct
  {
    halp::hslider_f32<"Rate", halp::range{0.01, 40., 5.}> rate;
  } inputs;
  struct
  {
  } outputs;
  void operator()() { }
};

//! ...which became a period: the field converts what was saved.
struct PeriodSpec
{
  halp_meta(name, "Rate to period")
  halp_meta(c_name, "rate_to_period_test")
  halp_meta(uuid, "5a4e8f0c-7d0b-4a51-9d7e-6c1f4b2e9a13");
  struct
  {
    struct : halp::time_chooser<"Period", halp::range{0.025, 100., 0.2}>
    {
      static float upgrade_value(float hz) { return hz > 0.f ? 1.f / hz : 0.2f; }
    } period;
  } inputs;
  struct
  {
  } outputs;
  void operator()() { }
};
}

TEST_CASE("A control whose unit changed converts the saved value", "[avnd][upgrade]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();

    oscr::ProcessModel<RateSpec> old{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{7778}, dctx, nullptr};
    auto* rate = qobject_cast<Process::FloatSlider*>(old.inlets()[0]);
    REQUIRE(rate);
    rate->setValue(4.f); // Hz

    JSONReader reader;
    reader.readFrom(static_cast<const Process::ProcessModel&>(old));
    const auto json = readJson(reader.toByteArray());
    JSONObject::Deserializer des{json};
    auto* loaded = new oscr::ProcessModel<PeriodSpec>{des, &doc->model()};
    auto* period = qobject_cast<Process::TimeChooser*>(loaded->inlets()[0]);
    REQUIRE(period);
    CHECK(period->id() == rate->id());
    CHECK(period->value() == ossia::value{ossia::vec2f{0.25f, 0.f}});
    delete loaded;
  });
}

#include <AvndProcesses/EntityToMidi.hpp>

TEST_CASE("Entity to MIDI: its times were milliseconds", "[avnd][upgrade]")
{
  using In = decltype(avnd_tools::EntityToMidi::inputs);
  // The hook the upgrade picks for each control that was a spinbox in ms
  CHECK(decltype(In::min_note)::upgrade_value(80.f) == 0.08f);
  CHECK(decltype(In::max_note)::upgrade_value(1500.f) == 1.5f);
  CHECK(decltype(In::trigger_duration)::upgrade_value(200.f) == 0.2f);
  CHECK(decltype(In::glide)::upgrade_value(60.f) == 0.06f);
  CHECK(decltype(In::max_hold)::upgrade_value(250.f) == 0.25f);
}

namespace
{
//! A knob with a mapper has a port type of its own, derived from the object and
//! the field...
struct MappedKnobSpec
{
  halp_meta(name, "Mapped knob to time chooser")
  halp_meta(c_name, "mapped_knob_to_time_chooser_test")
  halp_meta(uuid, "6b5f9a1d-8e1c-4b62-8e8f-7d205c3fab24");
  struct
  {
    struct : halp::knob_f32<"Decay", halp::range{0.005, 20., 0.35}>
    {
      using mapper = halp::log_mapper<std::ratio<85, 100>>;
    } decay;
    halp::knob_f32<"Level", halp::range{0., 2., 1.}> level;
  } inputs;
  struct
  {
  } outputs;
  void operator()() { }
};

//! ...which does not exist any more once it is a time chooser.
struct TimeChooserSpec
{
  halp_meta(name, "Mapped knob to time chooser")
  halp_meta(c_name, "mapped_knob_to_time_chooser_test")
  halp_meta(uuid, "6b5f9a1d-8e1c-4b62-8e8f-7d205c3fab24");
  struct
  {
    halp::time_chooser<"Decay", halp::range{0.005, 20., 0.35}> decay;
    halp::knob_f32<"Level", halp::range{0., 2., 1.}> level;
  } inputs;
  struct
  {
  } outputs;
  void operator()() { }
};
}

TEST_CASE(
    "A mapped knob, a port type of its own, that became a time chooser keeps its "
    "value",
    "[avnd][upgrade]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();

    oscr::ProcessModel<MappedKnobSpec> old{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{7779}, dctx, nullptr};
    auto* decay = qobject_cast<Process::ControlInlet*>(old.inlets()[0]);
    REQUIRE(decay);
    // Not one of the generic port types: nothing registers it
    CHECK(!ctx.interfaces<Process::PortFactoryList>().get(decay->concreteKey()));
    decay->setValue(3.2f);
    const State::AddressAccessor address{State::Address{"dev", {"decay"}}};
    decay->setAddress(address);
    auto* level = qobject_cast<Process::ControlInlet*>(old.inlets()[1]);
    REQUIRE(level);
    level->setValue(1.5f);

    SECTION("JSON")
    {
      JSONReader reader;
      reader.readFrom(static_cast<const Process::ProcessModel&>(old));
      const auto json = readJson(reader.toByteArray());
      JSONObject::Deserializer des{json};
      auto* loaded = new oscr::ProcessModel<TimeChooserSpec>{des, &doc->model()};
      REQUIRE(loaded->inlets().size() == 2);

      auto* chooser = qobject_cast<Process::TimeChooser*>(loaded->inlets()[0]);
      REQUIRE(chooser);
      CHECK(chooser->id() == decay->id());
      CHECK(chooser->value() == ossia::value{ossia::vec2f{3.2f, 0.f}});
      CHECK(chooser->address() == address);

      auto* loaded_level = qobject_cast<Process::ControlInlet*>(loaded->inlets()[1]);
      REQUIRE(loaded_level);
      CHECK(loaded_level->value() == ossia::value{1.5f});
      delete loaded;
    }

    SECTION("Binary")
    {
      // The binary format carries no field names: the unknown port is read up
      // to its Port base and rebuilt as the declared control, at its default.
      const QByteArray bytes
          = DataStreamReader::marshall(static_cast<const Process::ProcessModel&>(old));
      DataStream::Deserializer outer{bytes};
      QByteArray process;
      outer.stream() >> process;
      DataStream::Deserializer des{process};
      SCORE_DEBUG_CHECK_DELIMITER2(des);
      UuidKey<Process::ProcessModel> key;
      TSerializer<DataStream, UuidKey<Process::ProcessModel>>::writeTo(des, key);
      SCORE_DEBUG_CHECK_DELIMITER2(des);
      REQUIRE(key == static_cast<const Process::ProcessModel&>(old).concreteKey());
      auto* loaded = new oscr::ProcessModel<TimeChooserSpec>{des, &doc->model()};
      REQUIRE(loaded->inlets().size() == 2);

      auto* chooser = qobject_cast<Process::TimeChooser*>(loaded->inlets()[0]);
      REQUIRE(chooser);
      CHECK(chooser->id() == decay->id());
      CHECK(chooser->address() == address);

      auto* loaded_level = qobject_cast<Process::ControlInlet*>(loaded->inlets()[1]);
      REQUIRE(loaded_level);
      CHECK(loaded_level->value() == ossia::value{1.5f});
      delete loaded;
    }
  });
}
