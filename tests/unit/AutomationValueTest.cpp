#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <State/Address.hpp>

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>

#include <Curve/CurveConversion.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <Automation/AutomationModel.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

namespace
{
// Mirror of Automation::RecreateOnPlay::Component::on_curveChanged_impl<float>.
std::shared_ptr<ossia::curve<double, float>>
make_execution_curve(const Automation::ProcessModel& proc)
{
  const double min = proc.min();
  const double max = proc.max();

  auto scale_x = [](double val) -> double { return val; };
  auto scale_y = [=](double val) -> float { return val * (max - min) + min; };

  auto segt_data = proc.curve().sortedSegments();
  REQUIRE(!segt_data.empty());
  return Engine::score_to_ossia::curve<double, float>(scale_x, scale_y, segt_data, {});
}
}

TEST_CASE("Automation default curve scales into [min,max]", "[automation][values]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};

    Automation::ProcessModel proc{
        TimeVal::fromMsecs(1000.), Id<Process::ProcessModel>{1}, owner};

    // Fresh automation: a single default (power, linear-gamma) segment 0 -> 1.
    REQUIRE(proc.curve().segments().size() == 1);
    CHECK(proc.min() == 0.);
    CHECK(proc.max() == 1.);

    proc.setMin(2.);
    proc.setMax(6.);
    CHECK(proc.min() == 2.);
    CHECK(proc.max() == 6.);

    auto curve = make_execution_curve(proc);
    // out(t) = t * (6 - 2) + 2
    CHECK(curve->value_at(0.) == Approx(2.f).margin(1e-6));
    CHECK(curve->value_at(0.25) == Approx(3.f).margin(1e-6));
    CHECK(curve->value_at(0.5) == Approx(4.f).margin(1e-6));
    CHECK(curve->value_at(0.75) == Approx(5.f).margin(1e-6));
    CHECK(curve->value_at(1.) == Approx(6.f).margin(1e-6));

    // Inverted domain: min > max still follows the same affine formula.
    proc.setMin(10.);
    proc.setMax(0.);
    auto inv = make_execution_curve(proc);
    CHECK(inv->value_at(0.25) == Approx(7.5f).margin(1e-6));
  });
}

TEST_CASE("Automation multi-segment curve evaluates piecewise and clamps", "[automation][values]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};

    Automation::ProcessModel proc{
        TimeVal::fromMsecs(1000.), Id<Process::ProcessModel>{1}, owner};
    proc.setMin(0.);
    proc.setMax(10.);

    // Triangle: 0 -> 1 on [0, 0.5], 1 -> 0 on [0.5, 1].
    auto& cm = proc.curve();
    cm.clear();
    auto up = new Curve::LinearSegment{Id<Curve::SegmentModel>{1}, &cm};
    up->setStart({0., 0.});
    up->setEnd({0.5, 1.});
    auto down = new Curve::LinearSegment{Id<Curve::SegmentModel>{2}, &cm};
    down->setStart({0.5, 1.});
    down->setEnd({1., 0.});
    cm.addSegment(up);
    cm.addSegment(down);

    auto curve = make_execution_curve(proc);
    CHECK(curve->value_at(0.) == Approx(0.f).margin(1e-6));
    CHECK(curve->value_at(0.25) == Approx(5.f).margin(1e-6));
    CHECK(curve->value_at(0.5) == Approx(10.f).margin(1e-6));
    CHECK(curve->value_at(0.75) == Approx(5.f).margin(1e-6));
    CHECK(curve->value_at(1.) == Approx(0.f).margin(1e-6));

    // Out-of-domain t clamps to the curve boundary values.
    CHECK(curve->value_at(-0.5) == Approx(0.f).margin(1e-6));  // y0
    CHECK(curve->value_at(1.5) == Approx(0.f).margin(1e-6));   // last point
    CHECK(curve->value_at(0.5000001) == Approx(10.f).margin(1e-4));

    // Power curve: gamma 2 on [0,1] with domain [0, 8] => out(t) = 8 t^2.
    cm.clear();
    auto pw = new Curve::PowerSegment{Id<Curve::SegmentModel>{3}, &cm};
    pw->setStart({0., 0.});
    pw->setEnd({1., 1.});
    pw->gamma = 2.;
    cm.addSegment(pw);
    proc.setMax(8.);

    auto pcurve = make_execution_curve(proc);
    CHECK(pcurve->value_at(0.5) == Approx(2.f).margin(1e-6));
    CHECK(pcurve->value_at(0.25) == Approx(0.5f).margin(1e-6));
    CHECK(pcurve->value_at(1.) == Approx(8.f).margin(1e-6));

    // Empty curve: execution has no behavior to build (guarded upstream).
    cm.clear();
    CHECK(proc.curve().sortedSegments().empty());
  });
}

TEST_CASE("Automation serialization round-trips values, domain and address", "[automation][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};

    Automation::ProcessModel proc{
        TimeVal::fromMsecs(1000.), Id<Process::ProcessModel>{1}, owner};
    proc.setMin(-1.);
    proc.setMax(1.);
    proc.setTween(true);

    auto addr = State::Address::fromString("dev:/out");
    REQUIRE(addr);
    proc.setAddress(State::AddressAccessor{*addr});

    auto& cm = proc.curve();
    cm.clear();
    auto pw = new Curve::PowerSegment{Id<Curve::SegmentModel>{1}, &cm};
    pw->setStart({0., 0.});
    pw->setEnd({1., 1.});
    pw->gamma = 2.;
    cm.addSegment(pw);

    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
    const Process::ProcessModel& as_base = proc;

    const auto check_clone = [&](Process::ProcessModel* clone) {
      REQUIRE(clone);
      auto autom = dynamic_cast<Automation::ProcessModel*>(clone);
      REQUIRE(autom);

      CHECK(autom->min() == Approx(-1.).margin(1e-9));
      CHECK(autom->max() == Approx(1.).margin(1e-9));
      CHECK(autom->tween() == true);
      CHECK(autom->address() == State::AddressAccessor{*addr});

      // out(t) = 2 t^2 - 1 must survive the round-trip.
      auto curve = make_execution_curve(*autom);
      CHECK(curve->value_at(0.) == Approx(-1.f).margin(1e-6));
      CHECK(curve->value_at(0.5) == Approx(-0.5f).margin(1e-6));
      CHECK(curve->value_at(1.) == Approx(1.f).margin(1e-6));
    };

    // DataStream
    {
      const QByteArray bytes = score::marshall<DataStream>(as_base);
      auto clone = deserialize_interface(
          pl, DataStream::Deserializer{bytes}, doc->context(), owner);
      check_clone(clone);
      const Process::ProcessModel& clone_base = *clone;
      CHECK(score::marshall<DataStream>(clone_base) == bytes);
      delete clone;
    }

    // JSON
    {
      JSONReader reader;
      reader.readFrom(as_base);
      const rapidjson::Document jdoc = toValue(reader);
      JSONWriter writer{jdoc};
      auto clone = deserialize_interface(pl, writer, doc->context(), owner);
      check_clone(clone);

      JSONReader reader2;
      reader2.readFrom(static_cast<const Process::ProcessModel&>(*clone));
      CHECK(reader2.toByteArray() == reader.toByteArray());
      delete clone;
    }
  });
}
