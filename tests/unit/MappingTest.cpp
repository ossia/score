#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>

#include <Curve/CurveConversion.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <Mapping/MappingModel.hpp>

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
// Mirror of Mapping::RecreateOnPlay::Component::on_curveChanged_impl2<float,float>.
std::shared_ptr<ossia::curve<float, float>>
make_transfer_curve(const Mapping::ProcessModel& proc)
{
  const double xmin = proc.sourceMin();
  const double xmax = proc.sourceMax();
  const double ymin = proc.targetMin();
  const double ymax = proc.targetMax();

  auto scale_x = [=](double val) -> float { return val * (xmax - xmin) + xmin; };
  auto scale_y = [=](double val) -> float { return val * (ymax - ymin) + ymin; };

  auto segt_data = proc.curve().sortedSegments();
  REQUIRE(!segt_data.empty());
  return Engine::score_to_ossia::curve<float, float>(scale_x, scale_y, segt_data, {});
}
}

TEST_CASE("Mapping scales source domain onto target domain", "[mapping][values]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};

    Mapping::ProcessModel proc{
        TimeVal::fromMsecs(1000.), Id<Process::ProcessModel>{1}, owner};

    // Fresh mapping: identity curve on [0,1] -> [0,1].
    REQUIRE(proc.curve().segments().size() == 1);
    CHECK(proc.sourceMin() == 0.);
    CHECK(proc.sourceMax() == 1.);
    CHECK(proc.targetMin() == 0.);
    CHECK(proc.targetMax() == 1.);

    proc.setSourceMin(0.);
    proc.setSourceMax(100.);
    proc.setTargetMin(0.);
    proc.setTargetMax(1.);

    auto tf = make_transfer_curve(proc);
    // out(in) = in / 100 for the default linear curve.
    CHECK(tf->value_at(0.f) == Approx(0.f).margin(1e-6));
    CHECK(tf->value_at(25.f) == Approx(0.25f).margin(1e-6));
    CHECK(tf->value_at(50.f) == Approx(0.5f).margin(1e-6));
    CHECK(tf->value_at(100.f) == Approx(1.f).margin(1e-6));

    // Out-of-domain inputs clamp to the curve boundaries.
    CHECK(tf->value_at(-10.f) == Approx(0.f).margin(1e-6));
    CHECK(tf->value_at(200.f) == Approx(1.f).margin(1e-6));

    // Shifted domains: [50, 150] -> [-1, 1]: out(in) = (in-50)/100 * 2 - 1.
    proc.setSourceMin(50.);
    proc.setSourceMax(150.);
    proc.setTargetMin(-1.);
    proc.setTargetMax(1.);
    auto tf2 = make_transfer_curve(proc);
    CHECK(tf2->value_at(50.f) == Approx(-1.f).margin(1e-6));
    CHECK(tf2->value_at(100.f) == Approx(0.f).margin(1e-6));
    CHECK(tf2->value_at(125.f) == Approx(0.5f).margin(1e-6));
    CHECK(tf2->value_at(150.f) == Approx(1.f).margin(1e-6));

    // Inverted target domain: [0,100] -> [1,0]: out(in) = 1 - in/100.
    proc.setSourceMin(0.);
    proc.setSourceMax(100.);
    proc.setTargetMin(1.);
    proc.setTargetMax(0.);
    auto tf3 = make_transfer_curve(proc);
    CHECK(tf3->value_at(25.f) == Approx(0.75f).margin(1e-6));
    CHECK(tf3->value_at(100.f) == Approx(0.f).margin(1e-6));
  });
}

TEST_CASE("Mapping applies non-linear curve shapes to the input", "[mapping][values]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};

    Mapping::ProcessModel proc{
        TimeVal::fromMsecs(1000.), Id<Process::ProcessModel>{1}, owner};
    proc.setSourceMin(0.);
    proc.setSourceMax(100.);
    proc.setTargetMin(0.);
    proc.setTargetMax(1.);

    // gamma-2 power curve: out(in) = (in/100)^2.
    auto& cm = proc.curve();
    cm.clear();
    auto pw = new Curve::PowerSegment{Id<Curve::SegmentModel>{1}, &cm};
    pw->setStart({0., 0.});
    pw->setEnd({1., 1.});
    pw->gamma = 2.;
    cm.addSegment(pw);

    auto tf = make_transfer_curve(proc);
    CHECK(tf->value_at(50.f) == Approx(0.25f).margin(1e-6));
    CHECK(tf->value_at(25.f) == Approx(0.0625f).margin(1e-6));
    CHECK(tf->value_at(100.f) == Approx(1.f).margin(1e-6));

    cm.clear();
    auto up = new Curve::LinearSegment{Id<Curve::SegmentModel>{2}, &cm};
    up->setStart({0., 0.});
    up->setEnd({0.5, 1.});
    auto down = new Curve::LinearSegment{Id<Curve::SegmentModel>{3}, &cm};
    down->setStart({0.5, 1.});
    down->setEnd({1., 0.});
    cm.addSegment(up);
    cm.addSegment(down);

    auto tri = make_transfer_curve(proc);
    CHECK(tri->value_at(25.f) == Approx(0.5f).margin(1e-6));
    CHECK(tri->value_at(50.f) == Approx(1.f).margin(1e-6));
    CHECK(tri->value_at(75.f) == Approx(0.5f).margin(1e-6));
  });
}

TEST_CASE("Mapping serialization round-trips domains and transfer values", "[mapping][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};

    Mapping::ProcessModel proc{
        TimeVal::fromMsecs(1000.), Id<Process::ProcessModel>{1}, owner};
    proc.setSourceMin(-10.);
    proc.setSourceMax(10.);
    proc.setTargetMin(100.);
    proc.setTargetMax(200.);

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
      auto mapping = dynamic_cast<Mapping::ProcessModel*>(clone);
      REQUIRE(mapping);

      CHECK(mapping->sourceMin() == Approx(-10.).margin(1e-9));
      CHECK(mapping->sourceMax() == Approx(10.).margin(1e-9));
      CHECK(mapping->targetMin() == Approx(100.).margin(1e-9));
      CHECK(mapping->targetMax() == Approx(200.).margin(1e-9));

      // out(in) = 100 + 100 * ((in + 10) / 20)^2
      auto tf = make_transfer_curve(*mapping);
      CHECK(tf->value_at(-10.f) == Approx(100.f).margin(1e-4));
      CHECK(tf->value_at(0.f) == Approx(125.f).margin(1e-4));
      CHECK(tf->value_at(10.f) == Approx(200.f).margin(1e-4));
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
