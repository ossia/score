// L1 test harness — curve segment sweep.
//
// Iterates the entire Curve::SegmentList and, for EVERY registered curve
// segment factory (Linear, Power, PointArray + the full easing set):
//   1. constructs the segment via factory.make(id, parent);
//   2. asserts it constructs non-null;
//   3. JSON round-trip: serialize -> deserialize -> same concreteKey + a
//      second-pass byte fixed-point;
//   4. DataStream round-trip: same via the binary path.
//
// Segments are pure-math value objects (no external resource), so no crash
// guard is needed here — a plain try/catch is enough.

#include <score_test/App.hpp>

#include <Curve/Segment/CurveSegmentList.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>

#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <QObject>
#include <QString>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace
{
struct SegResult
{
  std::string uuid;
  std::string name;
  bool constructed = false;
  bool json_roundtrip = false;
  bool json_fixedpoint = false;
  bool datastream_roundtrip = false;
  bool datastream_fixedpoint = false;
  std::string detail;
};
}

TEST_CASE("Every curve segment factory instantiates and round-trips", "[nodes][l1][curve][serialization]")
{
  std::vector<SegResult> results;

  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto& sl = ctx.interfaces<Curve::SegmentList>();
    REQUIRE(!sl.empty());

    QObject parent; // standalone owner for the constructed segments
    int id_counter = 1;

    for(auto& factory : sl)
    {
      SegResult r;
      r.uuid = score::uuids::toByteArray(factory.concreteKey().impl()).toStdString();
      r.name = factory.prettyName().toStdString();

      Curve::SegmentModel* seg = nullptr;
      try
      {
        seg = factory.make(Id<Curve::SegmentModel>{id_counter++}, &parent);
      }
      catch(const std::exception& e)
      {
        r.detail = std::string("make() threw: ") + e.what();
      }
      catch(...)
      {
        r.detail = "make() threw (unknown)";
      }

      if(!seg)
      {
        results.push_back(std::move(r));
        continue;
      }
      r.constructed = true;

      // DataStream round-trip
      try
      {
        const QByteArray bytes = score::marshall<DataStream>(*seg);
        Curve::SegmentModel* clone
            = deserialize_interface(sl, DataStream::Deserializer{bytes}, &parent);
        if(clone)
        {
          r.datastream_roundtrip = (clone->concreteKey() == seg->concreteKey());
          r.datastream_fixedpoint = (score::marshall<DataStream>(*clone) == bytes);
          delete clone;
        }
        else
          r.detail += " [datastream: null]";
      }
      catch(const std::exception& e)
      {
        r.detail += std::string(" [datastream threw: ") + e.what() + "]";
      }
      catch(...)
      {
        r.detail += " [datastream threw unknown]";
      }

      // JSON round-trip
      try
      {
        JSONReader reader;
        reader.readFrom(*seg);
        const rapidjson::Document jdoc = toValue(reader);
        JSONWriter writer{jdoc};
        Curve::SegmentModel* clone = deserialize_interface(sl, writer, &parent);
        if(clone)
        {
          r.json_roundtrip = (clone->concreteKey() == seg->concreteKey());
          JSONReader reader2;
          reader2.readFrom(*clone);
          r.json_fixedpoint = (reader2.toByteArray() == reader.toByteArray());
          delete clone;
        }
        else
          r.detail += " [json: null]";
      }
      catch(const std::exception& e)
      {
        r.detail += std::string(" [json threw: ") + e.what() + "]";
      }
      catch(...)
      {
        r.detail += " [json threw unknown]";
      }

      delete seg;
      results.push_back(std::move(r));
    }
  });

  REQUIRE(!results.empty());

  int passed = 0;
  for(const auto& r : results)
  {
    INFO("Segment: " << r.name << "  {" << r.uuid << "}");
    if(!r.detail.empty())
      INFO("detail:" << r.detail);

    CHECK(r.constructed);
    CHECK(r.datastream_roundtrip);
    CHECK(r.datastream_fixedpoint);
    CHECK(r.json_roundtrip);
    CHECK(r.json_fixedpoint);

    if(r.constructed && r.datastream_roundtrip && r.datastream_fixedpoint
       && r.json_roundtrip && r.json_fixedpoint)
      ++passed;
  }

  WARN("Curve segment sweep: " << results.size() << " factories, " << passed
                               << " fully green.");
}
