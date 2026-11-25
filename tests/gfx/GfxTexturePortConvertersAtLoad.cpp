#include <score_test/App.hpp>

#include <Gfx/TexturePort.hpp>

#include <QMetaType>
#include <QVariant>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("the texture inlet converters are registered once the gfx library is loaded", "[gfx][js]")
{
  bool filter{}, address{}, format{};
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    QVariant f{1};
    filter = f.convert(QMetaType::fromType<std::optional<ossia::texture_filter>>())
             && f.value<std::optional<ossia::texture_filter>>()
                    == ossia::texture_filter::NEAREST;
    QVariant a{QStringLiteral("Mirror")};
    address = a.convert(QMetaType::fromType<std::optional<ossia::texture_address_mode>>())
              && a.value<std::optional<ossia::texture_address_mode>>()
                     == ossia::texture_address_mode::MIRROR;
    QVariant t{QStringLiteral("RGBA16F")};
    format = t.convert(QMetaType::fromType<std::optional<ossia::texture_format>>())
             && t.value<std::optional<ossia::texture_format>>()
                    == ossia::texture_format::RGBA16F;
  });
  CHECK(filter);
  CHECK(address);
  CHECK(format);
}
