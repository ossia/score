// Serialization of Gfx::OutputMapping — the per-output record of the
// multi-window device: source rect, target window, soft-edge blend, corner
// warp, lock mode, rotation and mirroring.
//
// The JSON reader omits every field that still holds its default, so a
// round-trip only proves anything when both the "written" and the "omitted"
// side of each of those branches is exercised. The writer additionally has to
// cope with documents that predate the LockMode enum (bool LockSizeToInput /
// Locked) and with arrays of the wrong arity.

#include <Gfx/Window/WindowSettings.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <core/application/MockApplication.hpp>

#include <catch2/catch_test_macros.hpp>

template <>
void JSONReader::read(const Gfx::OutputMapping& n);
template <>
void JSONWriter::write(Gfx::OutputMapping& n);
template <>
void DataStreamReader::read(const Gfx::OutputMapping& n);
template <>
void DataStreamWriter::write(Gfx::OutputMapping& n);

namespace
{
// Serializers dereference AppComponents(); must outlive every test.
const score::testing::MockApplication g_mock_app;

Gfx::OutputMapping jsonRoundTrip(const Gfx::OutputMapping& m)
{
  const QByteArray bytes = toJson(m);
  REQUIRE(!bytes.isEmpty());
  return fromJson<Gfx::OutputMapping>(bytes);
}

Gfx::OutputMapping dsRoundTrip(const Gfx::OutputMapping& m)
{
  const QByteArray bytes = score::marshall<DataStream>(m);
  REQUIRE(!bytes.isEmpty());
  return score::unmarshall<Gfx::OutputMapping>(bytes);
}

// Every field set to something that is NOT its default.
Gfx::OutputMapping fullyPopulated()
{
  Gfx::OutputMapping m;
  m.sourceRect = QRectF{0.25, 0.5, 0.5, 0.25};
  m.screenIndex = 2;
  m.windowPosition = QPoint{1920, -120};
  m.windowSize = QSize{3840, 2160};
  m.fullscreen = true;
  m.blendLeft = {0.125f, 1.5f};
  m.blendRight = {0.25f, 2.0f};
  m.blendTop = {0.0625f, 2.4f};
  m.blendBottom = {0.5f, 1.0f};
  m.cornerWarp.topLeft = {0.01, 0.02};
  m.cornerWarp.topRight = {0.98, 0.03};
  m.cornerWarp.bottomLeft = {0.04, 0.97};
  m.cornerWarp.bottomRight = {0.96, 0.95};
  m.lockMode = Gfx::OutputLockMode::AspectRatio;
  m.rotation = 270;
  m.mirrorX = true;
  m.mirrorY = true;
  return m;
}

void checkEquivalent(const Gfx::OutputMapping& a, const Gfx::OutputMapping& b)
{
  CHECK(a.sourceRect == b.sourceRect);
  CHECK(a.screenIndex == b.screenIndex);
  CHECK(a.windowPosition == b.windowPosition);
  CHECK(a.windowSize == b.windowSize);
  CHECK(a.fullscreen == b.fullscreen);
  CHECK(a.blendLeft.width == b.blendLeft.width);
  CHECK(a.blendLeft.gamma == b.blendLeft.gamma);
  CHECK(a.blendRight.width == b.blendRight.width);
  CHECK(a.blendRight.gamma == b.blendRight.gamma);
  CHECK(a.blendTop.width == b.blendTop.width);
  CHECK(a.blendTop.gamma == b.blendTop.gamma);
  CHECK(a.blendBottom.width == b.blendBottom.width);
  CHECK(a.blendBottom.gamma == b.blendBottom.gamma);
  CHECK(a.cornerWarp.topLeft == b.cornerWarp.topLeft);
  CHECK(a.cornerWarp.topRight == b.cornerWarp.topRight);
  CHECK(a.cornerWarp.bottomLeft == b.cornerWarp.bottomLeft);
  CHECK(a.cornerWarp.bottomRight == b.cornerWarp.bottomRight);
  CHECK(a.lockMode == b.lockMode);
  CHECK(a.rotation == b.rotation);
  CHECK(a.mirrorX == b.mirrorX);
  CHECK(a.mirrorY == b.mirrorY);
}
}

TEST_CASE("CornerWarp identity predicate", "[gfx][window][outputmapping]")
{
  CHECK(Gfx::CornerWarp{}.isIdentity());

  SECTION("each corner independently breaks identity")
  {
    // One coordinate at a time: isIdentity() is a conjunction of eight
    // comparisons and a typo in any one of them is invisible otherwise.
    const QPointF nudge{0.5, 0.5};
    {
      Gfx::CornerWarp w;
      w.topLeft = nudge;
      CHECK_FALSE(w.isIdentity());
    }
    {
      Gfx::CornerWarp w;
      w.topRight = nudge;
      CHECK_FALSE(w.isIdentity());
    }
    {
      Gfx::CornerWarp w;
      w.bottomLeft = nudge;
      CHECK_FALSE(w.isIdentity());
    }
    {
      Gfx::CornerWarp w;
      w.bottomRight = nudge;
      CHECK_FALSE(w.isIdentity());
    }
  }

  SECTION("sub-epsilon deviation still counts as identity")
  {
    Gfx::CornerWarp w;
    w.topLeft = {1e-9, -1e-9};
    w.bottomRight = {1.0 + 1e-9, 1.0 - 1e-9};
    CHECK(w.isIdentity());
  }
}

TEST_CASE("OutputMapping JSON round-trip", "[gfx][window][outputmapping]")
{
  SECTION("defaults")
  {
    // Exercises the omit-if-default side of CornerWarp / LockMode / Rotation /
    // MirrorX / MirrorY, and the absent-key side of every tryGet().
    const Gfx::OutputMapping def;
    checkEquivalent(def, jsonRoundTrip(def));
  }

  SECTION("every field non-default")
  {
    const auto m = fullyPopulated();
    checkEquivalent(m, jsonRoundTrip(m));
  }

  SECTION("each lock mode survives")
  {
    for(auto mode :
        {Gfx::OutputLockMode::Free, Gfx::OutputLockMode::AspectRatio,
         Gfx::OutputLockMode::OneToOne, Gfx::OutputLockMode::FullLock})
    {
      Gfx::OutputMapping m;
      m.lockMode = mode;
      CHECK(jsonRoundTrip(m).lockMode == mode);
    }
  }

  SECTION("each rotation survives")
  {
    for(int rot : {0, 90, 180, 270})
    {
      Gfx::OutputMapping m;
      m.rotation = rot;
      CHECK(jsonRoundTrip(m).rotation == rot);
    }
  }

  SECTION("mirror flags are independent")
  {
    for(bool x : {false, true})
      for(bool y : {false, true})
      {
        Gfx::OutputMapping m;
        m.mirrorX = x;
        m.mirrorY = y;
        const auto out = jsonRoundTrip(m);
        CHECK(out.mirrorX == x);
        CHECK(out.mirrorY == y);
      }
  }

  SECTION("an identity warp is not written, a non-identity one is")
  {
    Gfx::OutputMapping identity;
    CHECK_FALSE(QString::fromUtf8(toJson(identity)).contains("CornerWarp"));

    Gfx::OutputMapping warped;
    warped.cornerWarp.bottomRight = {0.75, 0.9};
    CHECK(QString::fromUtf8(toJson(warped)).contains("CornerWarp"));
    CHECK(jsonRoundTrip(warped).cornerWarp.bottomRight == QPointF{0.75, 0.9});
  }
}

TEST_CASE("OutputMapping JSON reads legacy documents", "[gfx][window][outputmapping]")
{
  // Documents written before OutputLockMode existed carry two booleans.
  SECTION("Locked maps to FullLock")
  {
    const auto m = fromJson<Gfx::OutputMapping>(R"({"Locked": true})");
    CHECK(m.lockMode == Gfx::OutputLockMode::FullLock);
  }

  SECTION("LockSizeToInput maps to OneToOne")
  {
    const auto m = fromJson<Gfx::OutputMapping>(R"({"LockSizeToInput": true})");
    CHECK(m.lockMode == Gfx::OutputLockMode::OneToOne);
  }

  SECTION("Locked wins over LockSizeToInput")
  {
    const auto m = fromJson<Gfx::OutputMapping>(
        R"({"Locked": true, "LockSizeToInput": true})");
    CHECK(m.lockMode == Gfx::OutputLockMode::FullLock);
  }

  SECTION("neither flag set stays Free")
  {
    const auto m = fromJson<Gfx::OutputMapping>(
        R"({"Locked": false, "LockSizeToInput": false})");
    CHECK(m.lockMode == Gfx::OutputLockMode::Free);
  }

  SECTION("an explicit LockMode ignores the legacy booleans")
  {
    const auto m = fromJson<Gfx::OutputMapping>(
        R"({"LockMode": 1, "Locked": true, "LockSizeToInput": true})");
    CHECK(m.lockMode == Gfx::OutputLockMode::AspectRatio);
  }
}

TEST_CASE("OutputMapping JSON tolerates malformed arrays", "[gfx][window][outputmapping]")
{
  // Arrays of the wrong arity must leave the field at its default rather than
  // read out of bounds.
  const Gfx::OutputMapping def;

  SECTION("short SourceRect")
  {
    const auto m = fromJson<Gfx::OutputMapping>(R"({"SourceRect": [0.0, 1.0]})");
    CHECK(m.sourceRect == def.sourceRect);
  }

  SECTION("long WindowPosition")
  {
    const auto m = fromJson<Gfx::OutputMapping>(R"({"WindowPosition": [1, 2, 3]})");
    CHECK(m.windowPosition == def.windowPosition);
  }

  SECTION("short WindowSize")
  {
    const auto m = fromJson<Gfx::OutputMapping>(R"({"WindowSize": [640]})");
    CHECK(m.windowSize == def.windowSize);
  }

  SECTION("short blend pair")
  {
    const auto m = fromJson<Gfx::OutputMapping>(R"({"BlendLeft": [0.5]})");
    CHECK(m.blendLeft.width == def.blendLeft.width);
    CHECK(m.blendLeft.gamma == def.blendLeft.gamma);
  }

  SECTION("short CornerWarp")
  {
    const auto m = fromJson<Gfx::OutputMapping>(
        R"({"CornerWarp": [0.0, 0.0, 1.0, 0.0, 0.0, 1.0]})");
    CHECK(m.cornerWarp.isIdentity());
  }

  SECTION("a well-formed CornerWarp is taken")
  {
    const auto m = fromJson<Gfx::OutputMapping>(
        R"({"CornerWarp": [0.1, 0.2, 0.8, 0.3, 0.2, 0.7, 0.9, 0.6]})");
    CHECK(m.cornerWarp.topLeft == QPointF{0.1, 0.2});
    CHECK(m.cornerWarp.topRight == QPointF{0.8, 0.3});
    CHECK(m.cornerWarp.bottomLeft == QPointF{0.2, 0.7});
    CHECK(m.cornerWarp.bottomRight == QPointF{0.9, 0.6});
  }

  SECTION("an empty object leaves every field default")
  {
    checkEquivalent(def, fromJson<Gfx::OutputMapping>(R"({})"));
  }
}

TEST_CASE("OutputMapping DataStream round-trip", "[gfx][window][outputmapping]")
{
  SECTION("defaults")
  {
    const Gfx::OutputMapping def;
    checkEquivalent(def, dsRoundTrip(def));
  }

  SECTION("every field non-default")
  {
    const auto m = fullyPopulated();
    checkEquivalent(m, dsRoundTrip(m));
  }

  SECTION("the binary form is unconditional, unlike JSON")
  {
    // Nothing is omitted, so two records that differ only in a defaulted field
    // still serialize to the same number of bytes.
    Gfx::OutputMapping a;
    Gfx::OutputMapping b;
    b.rotation = 180;
    b.lockMode = Gfx::OutputLockMode::FullLock;
    CHECK(score::marshall<DataStream>(a).size()
          == score::marshall<DataStream>(b).size());
  }
}
