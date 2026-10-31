// A script assigns a texture inlet's sampler settings as numbers or names:
// `inlet.textureMipmapMode = 1` threw "Cannot assign int to
// std::optional<texture_filter>" because nothing converted to the optional.
#include <Gfx/TexturePort.hpp>

#include <QMetaType>
#include <QVariant>

#include <catch2/catch_test_macros.hpp>

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
