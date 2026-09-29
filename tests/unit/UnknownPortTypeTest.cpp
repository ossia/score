// A process's saved ports whose port type is not registered (the plug-in that
// provided it is gone, or the process now declares another kind of port there)
// load as the generic port of their direction instead of aborting the load:
// same id (cables and the process's port mapping refer to it), name and
// address, and from a JSON save a control keeps its value and domain. Ports of
// a known type in the same list load as usual.

#include <State/Address.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QObject>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

#include <typeinfo>

namespace
{
// Port types no plug-in registers
const char* const unknown_uuids[]
    = {"2a3e5b1c-0d4f-4e6a-8b7c-1d2e3f405162", "3b4f6c2d-1e50-4f7b-9c8d-2e3f40516273",
       "4c507d3e-2f61-4a8c-8d9e-3f4051627384", "5d618e4f-3072-4b9d-9eaf-405162738495"};

struct Saved
{
  QObject owner;
  Process::Inlets inlets;
  Process::Outlets outlets;
  const State::AddressAccessor address{State::Address{"dev", {"level"}}};

  Saved()
  {
    auto level
        = new Process::FloatSlider{0.f, 2.f, 1.f, "Level", Id<Process::Port>{3}, &owner};
    level->setValue(0.4f);
    level->setAddress(address);
    inlets.push_back(level);

    auto in = new Process::ValueInlet{"In", Id<Process::Port>{4}, &owner};
    in->setAddress(address);
    inlets.push_back(in);

    inlets.push_back(new Process::MidiInlet{"Notes", Id<Process::Port>{7}, &owner});

    auto out = new Process::ControlOutlet{"Out", Id<Process::Port>{5}, &owner};
    out->setValue(0.7f);
    outlets.push_back(out);

    outlets.push_back(new Process::ValueOutlet{"Values", Id<Process::Port>{6}, &owner});
  }

  //! The ports whose type is replaced by an unknown one
  std::vector<Process::Port*> unknown() const
  {
    return {inlets[0], inlets[1], outlets[0], outlets[1]};
  }
};

template <typename T>
bool is_exactly(const QObject* p)
{
  return p && typeid(*p) == typeid(T);
}

void check_common(const Process::Port* loaded, const Process::Port& saved)
{
  REQUIRE(loaded);
  CHECK(loaded->id() == saved.id());
  CHECK(loaded->name() == saved.name());
  CHECK(loaded->address() == saved.address());
}
}

TEST_CASE("Ports of an unknown type load as generic ports (JSON)", "[process][ports]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Saved saved;
    JSONReader reader;
    reader.stream.StartObject();
    Process::readPorts(reader, saved.inlets, saved.outlets);
    reader.stream.EndObject();
    auto json = readJson(reader.toByteArray());

    int i = 0;
    auto& alloc = json.GetAllocator();
    for(auto* port : saved.unknown())
    {
      auto& list = json[qobject_cast<Process::Inlet*>(port) ? "Inlets" : "Outlets"];
      for(auto& p : list.GetArray())
        if(p["id"].GetInt() == port->id().val())
          p["uuid"].SetString(unknown_uuids[i], alloc);
      i++;
    }

    QObject parent;
    Process::Inlets inlets;
    Process::Outlets outlets;
    Process::writePorts(
        JSONObject::Deserializer{json}, ctx.interfaces<Process::PortFactoryList>(),
        inlets, outlets, &parent);

    REQUIRE(inlets.size() == 3);
    REQUIRE(outlets.size() == 2);

    // A control keeps what makes it one
    REQUIRE(is_exactly<Process::ControlInlet>(inlets[0]));
    check_common(inlets[0], *saved.inlets[0]);
    auto* level = static_cast<Process::ControlInlet*>(inlets[0]);
    CHECK(level->value() == ossia::value{0.4f});
    CHECK(
        level->domain()
        == static_cast<Process::ControlInlet*>(saved.inlets[0])->domain());

    CHECK(is_exactly<Process::ValueInlet>(inlets[1]));
    check_common(inlets[1], *saved.inlets[1]);

    // Known: untouched
    CHECK(is_exactly<Process::MidiInlet>(inlets[2]));
    check_common(inlets[2], *saved.inlets[2]);

    REQUIRE(is_exactly<Process::ControlOutlet>(outlets[0]));
    check_common(outlets[0], *saved.outlets[0]);
    CHECK(
        static_cast<Process::ControlOutlet*>(outlets[0])->value() == ossia::value{0.7f});

    CHECK(is_exactly<Process::ValueOutlet>(outlets[1]));
    check_common(outlets[1], *saved.outlets[1]);

    for(auto* p : inlets)
      CHECK(p->parent() == &parent);
    for(auto* p : outlets)
      CHECK(p->parent() == &parent);
  });
}

TEST_CASE("Ports of an unknown type load as generic ports (binary)", "[process][ports]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Saved saved;
    QByteArray bytes;
    {
      DataStreamReader reader{&bytes};
      Process::readPorts(reader, saved.inlets, saved.outlets);
    }

    // The keys are stored as their 16 raw bytes
    int i = 0;
    for(auto* port : saved.unknown())
    {
      const auto& known = port->concreteKey().impl();
      const auto unknown = score::uuids::string_generator::compute(
          unknown_uuids[i], unknown_uuids[i] + 36);
      const QByteArray from{(const char*)known.data, sizeof(known.data)};
      const QByteArray to{(const char*)unknown.data, sizeof(unknown.data)};
      REQUIRE(bytes.count(from) == 1);
      bytes.replace(from, to);
      i++;
    }

    QObject parent;
    Process::Inlets inlets;
    Process::Outlets outlets;
    DataStream::Deserializer writer{bytes};
    Process::writePorts(
        writer, ctx.interfaces<Process::PortFactoryList>(), inlets, outlets, &parent);

    REQUIRE(inlets.size() == 3);
    REQUIRE(outlets.size() == 2);

    // What follows the Port base in the binary format depends on the unknown
    // type: only the base is kept.
    CHECK(is_exactly<Process::ValueInlet>(inlets[0]));
    check_common(inlets[0], *saved.inlets[0]);
    CHECK(is_exactly<Process::ValueInlet>(inlets[1]));
    check_common(inlets[1], *saved.inlets[1]);
    CHECK(is_exactly<Process::MidiInlet>(inlets[2]));
    check_common(inlets[2], *saved.inlets[2]);
    CHECK(is_exactly<Process::ValueOutlet>(outlets[0]));
    check_common(outlets[0], *saved.outlets[0]);
    CHECK(is_exactly<Process::ValueOutlet>(outlets[1]));
    check_common(outlets[1], *saved.outlets[1]);
  });
}
