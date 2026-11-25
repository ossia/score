#include <Gfx/TexturePort.hpp>

#include <QMetaType>
#include <QVariant>

#include <catch2/catch_test_macros.hpp>

#include <limits>

TEST_CASE("texture inlet sampler settings convert from script values", "[gfx][js]")
{
  Gfx::registerTexturePortConverters();
  using filter = std::optional<ossia::texture_filter>;
  using address = std::optional<ossia::texture_address_mode>;

  QVariant v{1};
  REQUIRE(v.convert(QMetaType::fromType<filter>()));
  CHECK(v.value<filter>() == ossia::texture_filter::NEAREST);

  QVariant d{2.0};
  REQUIRE(d.convert(QMetaType::fromType<filter>()));
  CHECK(d.value<filter>() == ossia::texture_filter::LINEAR);

  QVariant s{QStringLiteral("linear")};
  REQUIRE(s.convert(QMetaType::fromType<filter>()));
  CHECK(s.value<filter>() == ossia::texture_filter::LINEAR);

  QVariant none{-1};
  REQUIRE(none.convert(QMetaType::fromType<filter>()));
  CHECK(!none.value<filter>().has_value());

  QVariant a{QStringLiteral("Mirror")};
  REQUIRE(a.convert(QMetaType::fromType<address>()));
  CHECK(a.value<address>() == ossia::texture_address_mode::MIRROR);
}

TEST_CASE("non-finite and out-of-range numbers clear a texture setting", "[gfx][js]")
{
  Gfx::registerTexturePortConverters();
  using filter = std::optional<ossia::texture_filter>;
  using address = std::optional<ossia::texture_address_mode>;
  using format = std::optional<ossia::texture_format>;

  for(double bad :
      {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
       -std::numeric_limits<double>::infinity(), 1e20, -1e20, 3e9})
  {
    QVariant f{bad};
    REQUIRE(f.convert(QMetaType::fromType<filter>()));
    CHECK(!f.value<filter>().has_value());
    QVariant a{bad};
    REQUIRE(a.convert(QMetaType::fromType<address>()));
    CHECK(!a.value<address>().has_value());
    QVariant t{bad};
    REQUIRE(t.convert(QMetaType::fromType<format>()));
    CHECK(!t.value<format>().has_value());
  }
}

TEST_CASE("texture inlet format converts from script values", "[gfx][js]")
{
  Gfx::registerTexturePortConverters();
  using format = std::optional<ossia::texture_format>;

  QVariant i{int(ossia::texture_format::RGBA16F)};
  REQUIRE(i.convert(QMetaType::fromType<format>()));
  CHECK(i.value<format>() == ossia::texture_format::RGBA16F);

  QVariant d{double(ossia::texture_format::R32F)};
  REQUIRE(d.convert(QMetaType::fromType<format>()));
  CHECK(d.value<format>() == ossia::texture_format::R32F);

  QVariant n{QStringLiteral("rgba32f")};
  REQUIRE(n.convert(QMetaType::fromType<format>()));
  CHECK(n.value<format>() == ossia::texture_format::RGBA32F);

  QVariant automatic{QStringLiteral("Auto")};
  REQUIRE(automatic.convert(QMetaType::fromType<format>()));
  CHECK(!automatic.value<format>().has_value());

  QVariant past{int(ossia::texture_format::D32F) + 1};
  REQUIRE(past.convert(QMetaType::fromType<format>()));
  CHECK(!past.value<format>().has_value());
}
