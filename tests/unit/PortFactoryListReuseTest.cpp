// Regression test: per-app interface-list lookup in port deserialization.
//
// Process::load_inlet / load_outlet (and a dozen similar deserialization
// sites) used to cache the application's PortFactoryList in a function-local
// `static`:
//
//   static auto& il = score::AppComponents().interfaces<Process::PortFactoryList>();
//
// The `static` froze the FIRST application's factory list. When a second
// score application was booted in the same process — exactly what the test
// fixtures do when several run_in_app tests share one binary — the first
// app's ApplicationComponents had been destroyed, and the second app's
// deserialization iterated the freed list: heap-use-after-free.
//
// This test boots two MinimalApplications sequentially and round-trips a
// ControlInlet / ControlOutlet through Process::load_inlet / load_outlet in
// each. Under ASAN it crashes in the second round without the fix (the fix
// looks the list up per-call through the serializer's `components` member).

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortSerialization.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <score_test/App.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
// Serialize + deserialize ports so that all four load_inlet/load_outlet
// overloads (JSON + DataStream) run against the current app's factory list.
void roundtrip_ports_in_fresh_app()
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Process::ControlInlet inlet{"Ctl In", Id<Process::Port>{1234}, nullptr};
    inlet.setValue(2.5f);

    Process::ControlOutlet outlet{"Ctl Out", Id<Process::Port>{5678}, nullptr};
    outlet.setValue(0.75f);

    // JSON round-trip
    {
      auto json = toValue(score::marshall<JSONObject>((Process::Inlet&)inlet));
      JSONObject::Deserializer writer{json};
      auto in = Process::load_inlet(writer, nullptr);
      REQUIRE(in);
      auto ctl = dynamic_cast<Process::ControlInlet*>(in.get());
      REQUIRE(ctl);
      CHECK(ctl->id() == inlet.id());
      CHECK(ctl->value() == inlet.value());
    }
    {
      auto json = toValue(score::marshall<JSONObject>((Process::Outlet&)outlet));
      JSONObject::Deserializer writer{json};
      auto out = Process::load_outlet(writer, nullptr);
      REQUIRE(out);
      auto ctl = dynamic_cast<Process::ControlOutlet*>(out.get());
      REQUIRE(ctl);
      CHECK(ctl->id() == outlet.id());
      CHECK(ctl->value() == outlet.value());
    }

    // DataStream round-trip
    {
      auto data = score::marshall<DataStream>((Process::Inlet&)inlet);
      DataStream::Deserializer writer{data};
      auto in = Process::load_inlet(writer, nullptr);
      REQUIRE(in);
      auto ctl = dynamic_cast<Process::ControlInlet*>(in.get());
      REQUIRE(ctl);
      CHECK(ctl->id() == inlet.id());
      CHECK(ctl->value() == inlet.value());
    }
    {
      auto data = score::marshall<DataStream>((Process::Outlet&)outlet);
      DataStream::Deserializer writer{data};
      auto out = Process::load_outlet(writer, nullptr);
      REQUIRE(out);
      auto ctl = dynamic_cast<Process::ControlOutlet*>(out.get());
      REQUIRE(ctl);
      CHECK(ctl->id() == outlet.id());
      CHECK(ctl->value() == outlet.value());
    }
  });
}
}

TEST_CASE(
    "Port deserialization uses the current app's factory list, not a stale "
    "static cache",
    "[unit][regression][serialization][ports]")
{
  // First app: primes what used to be the function-local static caches.
  roundtrip_ports_in_fresh_app();

  // Second app in the same process: before the fix, load_inlet/load_outlet
  // iterated the torn-down first app's PortFactoryList (heap-use-after-free).
  roundtrip_ports_in_fresh_app();

  SUCCEED("two apps deserialized ports in one process");
}
