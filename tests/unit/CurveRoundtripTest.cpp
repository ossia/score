// P3R2 value-processor unit tests — Curve::Model serialization round-trip.
//
// Curve deserialization resolves segment factories through the application
// components (Curve::SegmentList), so these cases run inside the headless app
// fixture (run_in_app / APP mode). Round-trips are asserted on VALUES: the
// reloaded curve must sample identically on a dense grid, and segment-specific
// data (power gamma) must survive both the JSON and DataStream paths.

#include <score_test/App.hpp>

#include <Curve/CurveModel.hpp>
#include <Curve/Segment/EasingSegment.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <QDataStream>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

// The QDataStream operators for the segment-specific data types are declared
// in the plugin headers but defined inside the hidden-visibility curve plugin;
// the Qt metatype instantiations triggered in this TU (QVariant::value<...>()
// in gamma_of, header-inline EasingSegment code) reference them at link time.
// Provide semantically-equivalent local definitions; score's own segment
// serialization goes through the factory path inside the plugin and never
// calls these.
QDataStream& operator<<(QDataStream& s, const Curve::PowerSegmentData& d)
{
  return s << d.gamma;
}
QDataStream& operator>>(QDataStream& s, Curve::PowerSegmentData& d)
{
  return s >> d.gamma;
}
QDataStream& operator<<(QDataStream& s, const Curve::EasingData&)
{
  return s;
}
QDataStream& operator>>(QDataStream& s, Curve::EasingData&)
{
  return s;
}

namespace
{
// A curve exercising three different segment kinds:
//   [0,   0.4] linear    0   -> 1
//   [0.4, 0.7] power^3   1   -> 0.2
//   [0.7, 1  ] quadIn    0.2 -> 0.9
void build_reference_curve(Curve::Model& model)
{
  auto lin = new Curve::LinearSegment{Id<Curve::SegmentModel>{1}, &model};
  lin->setStart({0., 0.});
  lin->setEnd({0.4, 1.});

  auto pow = new Curve::PowerSegment{Id<Curve::SegmentModel>{2}, &model};
  pow->setStart({0.4, 1.});
  pow->setEnd({0.7, 0.2});
  pow->gamma = 3.;

  auto ease = new Curve::Segment_quadraticIn{Id<Curve::SegmentModel>{3}, &model};
  ease->setStart({0.7, 0.2});
  ease->setEnd({1., 0.9});

  model.addSegment(lin);
  model.addSegment(pow);
  model.addSegment(ease);
}

void check_same_samples(const Curve::Model& a, const Curve::Model& b)
{
  for(int i = 0; i <= 100; i++)
  {
    const double x = i / 100.;
    const auto va = a.valueAt(x);
    const auto vb = b.valueAt(x);
    REQUIRE(va.has_value() == vb.has_value());
    if(va)
      CHECK(*va == Approx(*vb).margin(1e-12));
  }
}

double gamma_of(const Curve::Model& m)
{
  for(const auto& data : m.toCurveData())
    if(data.type == Metadata<ConcreteKey_k, Curve::PowerSegment>::get())
      return data.specificSegmentData.value<Curve::PowerSegmentData>().gamma;
  return -1.;
}
}

TEST_CASE("Curve::Model DataStream round-trip preserves sampled values", "[curve][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QObject parent;
    Curve::Model model{Id<Curve::Model>{1}, &parent};
    build_reference_curve(model);

    // Sanity on the source model itself.
    REQUIRE(model.segments().size() == 3);
    CHECK(*model.valueAt(0.2) == Approx(0.5).margin(1e-12));
    CHECK(gamma_of(model) == Approx(3.).margin(1e-12));

    const QByteArray bytes = score::marshall<DataStream>(model);
    DataStream::Deserializer des{bytes};
    Curve::Model reloaded{des, &parent};

    REQUIRE(reloaded.segments().size() == 3);
    CHECK(gamma_of(reloaded) == Approx(3.).margin(1e-12));
    check_same_samples(model, reloaded);

    // Byte fixed-point: serializing the clone yields identical bytes.
    CHECK(score::marshall<DataStream>(reloaded) == bytes);
  });
}

TEST_CASE("Curve::Model JSON round-trip preserves sampled values", "[curve][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QObject parent;
    Curve::Model model{Id<Curve::Model>{1}, &parent};
    build_reference_curve(model);

    JSONReader reader;
    reader.readFrom(model);
    const rapidjson::Document jdoc = toValue(reader);

    JSONObject::Deserializer des{jdoc};
    Curve::Model reloaded{des, &parent};

    REQUIRE(reloaded.segments().size() == 3);
    CHECK(gamma_of(reloaded) == Approx(3.).margin(1e-12));
    check_same_samples(model, reloaded);

    // JSON fixed-point.
    JSONReader reader2;
    reader2.readFrom(reloaded);
    CHECK(reader2.toByteArray() == reader.toByteArray());
  });
}
