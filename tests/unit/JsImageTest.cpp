#include <JS/Qml/Utils.hpp>

#include <QImage>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Script image sampling returns straight RGBA", "[unit][js][image]")
{
  JS::JsUtils util;
  QImage image{2, 1, QImage::Format_RGBA8888_Premultiplied};
  image.setPixelColor(0, 0, QColor{255, 128, 64, 128});
  image.setPixelColor(1, 0, QColor{16, 32, 48, 255});

  // Qt Quick captures premultiplied images; the picker must not return the
  // stored (128, 64, 32) bytes as the user's next brush colour.
  const auto sampled = util.imagePixelColor(image, 0, 0);
  CHECK(sampled.red() == 255);
  CHECK(sampled.green() == 128);
  CHECK(sampled.blue() == 64);
  CHECK(sampled.alpha() == 128);
  CHECK(util.imagePixelColor(image, 1, 0) == QColor{16, 32, 48, 255});

  // Straight-alpha images must not be unpremultiplied a second time.
  QImage straight{1, 1, QImage::Format_ARGB32};
  straight.setPixelColor(0, 0, QColor{255, 128, 64, 128});
  CHECK(util.imagePixelColor(straight, 0, 0).rgba() == qRgba(255, 128, 64, 128));
}

TEST_CASE(
    "Script image sampling bounds never address another pixel", "[unit][js][image]")
{
  JS::JsUtils util;
  QImage image{2, 2, QImage::Format_RGB32};
  image.fill(Qt::red);
  CHECK(util.imagePixelColor(image, -1, 0) == QColor{Qt::transparent});
  CHECK(util.imagePixelColor(image, 2, 0) == QColor{Qt::transparent});
  CHECK(util.imagePixelColor(image, 0, -1) == QColor{Qt::transparent});
  CHECK(util.imagePixelColor(image, 0, 2) == QColor{Qt::transparent});
  CHECK(util.imagePixelColor(QImage{}, 0, 0) == QColor{Qt::transparent});
}
