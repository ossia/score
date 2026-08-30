// JSON serialization of the shared-memory device settings of score-plugin-gfx.
//
// Gfx::SharedInputSettings and Gfx::SharedOutputSettings are not one protocol
// each: Spout, Syphon, Sh4lt, Shmdata and Pipewire in/out all store their
// device-specific settings in these two structs, so their JSON writers are the
// parser for ten protocols.
//
// A settings object does not only come from a .score written by the same
// build. It also comes from Score.createDevice(name, uuid, { ... }) in a user
// script, where every key is optional and every value is whatever the script
// put there. rapidjson's operator[] and GetString()/GetDouble() assert rather
// than fail, and assert() is live in this build, so an absent or mistyped key
// used to take the whole application down.

#include <Gfx/SharedInputSettings.hpp>
#include <Gfx/SharedOutputSettings.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <core/application/MockApplication.hpp>

#include <catch2/catch_test_macros.hpp>

template <>
void JSONReader::read(const Gfx::SharedInputSettings& n);
template <>
void JSONWriter::write(Gfx::SharedInputSettings& n);
template <>
void JSONReader::read(const Gfx::SharedOutputSettings& n);
template <>
void JSONWriter::write(Gfx::SharedOutputSettings& n);
template <>
void DataStreamReader::read(const Gfx::SharedOutputSettings& n);
template <>
void DataStreamWriter::write(Gfx::SharedOutputSettings& n);

namespace
{
const score::testing::MockApplication g_mock_app;

// These readers write bare members, the way a protocol's
// serializeProtocolSpecificSettings does: into an object the caller opened.
template <typename T>
QByteArray settingsToJson(const T& s)
{
  JSONReader r;
  r.stream.StartObject();
  r.read(s);
  r.stream.EndObject();
  return r.toByteArray();
}

template <typename T>
T roundTrip(const T& s)
{
  const QByteArray bytes = settingsToJson(s);
  REQUIRE(!bytes.isEmpty());
  return fromJson<T>(bytes);
}
}

TEST_CASE("SharedInputSettings JSON round-trip", "[gfx][device][settings]")
{
  Gfx::SharedInputSettings s;
  s.path = "/score-shm-input";

  CHECK(roundTrip(s).path == s.path);
}

TEST_CASE("SharedInputSettings survives hostile JSON", "[gfx][device][settings]")
{
  SECTION("an absent key leaves the default")
  {
    CHECK(fromJson<Gfx::SharedInputSettings>(R"({})").path == QString{});
  }

  SECTION("a key of the wrong type leaves the default")
  {
    CHECK(fromJson<Gfx::SharedInputSettings>(R"({"Path": 42})").path == QString{});
    CHECK(fromJson<Gfx::SharedInputSettings>(R"({"Path": null})").path == QString{});
    CHECK(fromJson<Gfx::SharedInputSettings>(R"({"Path": [1,2]})").path == QString{});
    CHECK(fromJson<Gfx::SharedInputSettings>(R"({"Path": {"a": 1}})").path == QString{});
    CHECK(fromJson<Gfx::SharedInputSettings>(R"({"Path": true})").path == QString{});
  }

  SECTION("unknown keys are ignored, known ones still read")
  {
    const auto s
        = fromJson<Gfx::SharedInputSettings>(R"({"Nope": 1, "Path": "/ok"})");
    CHECK(s.path == "/ok");
  }
}

TEST_CASE("SharedOutputSettings JSON round-trip", "[gfx][device][settings]")
{
  Gfx::SharedOutputSettings s;
  s.path = "/score-shm-output";
  s.width = 1920;
  s.height = 1080;
  s.rate = 59.94;

  const auto r = roundTrip(s);
  CHECK(r.path == s.path);
  CHECK(r.width == s.width);
  CHECK(r.height == s.height);
  CHECK(r.rate == s.rate);
}

TEST_CASE("SharedOutputSettings survives hostile JSON", "[gfx][device][settings]")
{
  const Gfx::SharedOutputSettings def;

  SECTION("an empty object leaves every field default")
  {
    const auto s = fromJson<Gfx::SharedOutputSettings>(R"({})");
    CHECK(s.path == def.path);
    CHECK(s.width == def.width);
    CHECK(s.height == def.height);
    CHECK(s.rate == def.rate);
  }

  SECTION("strings where numbers are expected leave the defaults")
  {
    const auto s = fromJson<Gfx::SharedOutputSettings>(
        R"({"Path": "/ok", "Width": "1920", "Height": [1], "Rate": null})");
    CHECK(s.path == "/ok");
    CHECK(s.width == def.width);
    CHECK(s.height == def.height);
    CHECK(s.rate == def.rate);
  }

  SECTION("a number where a string is expected leaves the default")
  {
    const auto s = fromJson<Gfx::SharedOutputSettings>(
        R"({"Path": 8, "Width": 640, "Height": 480, "Rate": 30})");
    CHECK(s.path == def.path);
    CHECK(s.width == 640);
    CHECK(s.height == 480);
    CHECK(s.rate == 30.);
  }

  SECTION("a partial object reads the keys that are present")
  {
    const auto s = fromJson<Gfx::SharedOutputSettings>(R"({"Height": 720})");
    CHECK(s.height == 720);
    CHECK(s.width == def.width);
  }
}

TEST_CASE("SharedOutputSettings DataStream round-trip", "[gfx][device][settings]")
{
  Gfx::SharedOutputSettings s;
  s.path = "/dev/shm/score";
  s.width = 800;
  s.height = 600;
  s.rate = 25.;

  const auto r
      = score::unmarshall<Gfx::SharedOutputSettings>(score::marshall<DataStream>(s));
  CHECK(r.path == s.path);
  CHECK(r.width == s.width);
  CHECK(r.height == s.height);
  CHECK(r.rate == s.rate);
}
