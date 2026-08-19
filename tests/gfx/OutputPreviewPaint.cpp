// Gfx/Window/OutputPreview.cpp — the on-screen preview of a multi-window
// output mapping. PreviewWidget paints the four preview contents through a
// warp + rotation/mirror transform and overlays the soft-edge blend gradients;
// OutputPreviewWindows keeps one such window per mapping in sync.
//
// All of it is QPainter work on a plain QWidget, so it is fully checkable
// offscreen: QWidget::grab() renders the widget through paintEvent and hands
// back the pixels. The blend overlay has a closed form — the shader it mirrors
// multiplies the content by pow(t, gamma) — so the gradient is asserted
// numerically rather than by "it changed".

#include <Gfx/Window/OutputPreview.hpp>
#include <Gfx/Window/WindowSettings.hpp>

#include <score_test/App.hpp>

#include <QMouseEvent>
#include <QPixmap>
#include <QScreen>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace Gfx;

namespace
{
constexpr int W = 240;
constexpr int H = 160;

QImage grab(PreviewWidget& w)
{
  w.resize(W, H);
  return w.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

QColor px(const QImage& img, int x, int y)
{
  return img.pixelColor(x, y);
}

bool isBlack(const QColor& c)
{
  return c.red() < 4 && c.green() < 4 && c.blue() < 4;
}

/// Mean over the whole image of the max channel; a cheap "how bright is it".
double meanLuma(const QImage& img)
{
  double acc = 0;
  for(int y = 0; y < img.height(); ++y)
    for(int x = 0; x < img.width(); ++x)
    {
      const auto c = img.pixelColor(x, y);
      acc += (c.red() + c.green() + c.blue()) / 3.0;
    }
  return acc / (img.width() * img.height());
}

int differingPixels(const QImage& a, const QImage& b)
{
  if(a.size() != b.size())
    return -1;
  int n = 0;
  for(int y = 0; y < a.height(); ++y)
    for(int x = 0; x < a.width(); ++x)
      if(a.pixel(x, y) != b.pixel(x, y))
        ++n;
  return n;
}

// The palette PreviewWidget uses for OutputContent::OutputIdentification.
// Duplicated on purpose: previewColorForIndex is a file-static, and a test that
// re-read the implementation's own table would assert nothing.
const QColor idColors[8]
    = {QColor(30, 60, 180),  QColor(180, 30, 30),  QColor(30, 150, 30),
       QColor(180, 150, 30), QColor(150, 30, 150), QColor(30, 150, 150),
       QColor(200, 100, 30), QColor(100, 30, 200)};
}

TEST_CASE("PreviewWidget content modes", "[gfx][window][preview]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    SECTION("black paints nothing but black")
    {
      PreviewWidget w{0, PreviewContent::Black};
      const auto img = grab(w);
      CHECK(isBlack(px(img, W / 2, H / 2)));
      CHECK(meanLuma(img) == Approx(0.0).margin(0.5));
    }

    SECTION("output identification paints its index colour")
    {
      for(int i = 0; i < 10; ++i)
      {
        PreviewWidget w{i, PreviewContent::OutputIdentification};
        const auto img = grab(w);
        // The centre carries the big index digit, so sample a corner region
        // that the text cannot reach.
        const auto c = px(img, 3, 3);
        INFO("index " << i);
        CHECK(c == idColors[i % 8]);
      }
    }

    SECTION("the per-output test card is not blank")
    {
      PreviewWidget w{0, PreviewContent::PerOutputTestCard};
      w.setOutputResolution({640, 480});
      const auto img = grab(w);
      CHECK(meanLuma(img) > 10.0);
    }

    SECTION("the global test card is black until one is supplied")
    {
      PreviewWidget w{0, PreviewContent::GlobalTestCard};
      const auto blank = grab(w);
      CHECK(meanLuma(blank) == Approx(0.0).margin(0.5));

      QImage card{64, 64, QImage::Format_ARGB32};
      card.fill(QColor(200, 100, 50));
      w.setGlobalTestCard(card);
      const auto filled = grab(w);
      CHECK(px(filled, W / 2, H / 2) == QColor(200, 100, 50));
    }

    SECTION("the global test card is sampled through the source rect")
    {
      QImage card{64, 64, QImage::Format_ARGB32};
      // Left half red, right half green.
      for(int y = 0; y < 64; ++y)
        for(int x = 0; x < 64; ++x)
          card.setPixelColor(x, y, x < 32 ? QColor(255, 0, 0) : QColor(0, 255, 0));

      PreviewWidget w{0, PreviewContent::GlobalTestCard};
      w.setGlobalTestCard(card);

      w.setSourceRect({0.0, 0.0, 0.5, 1.0});
      CHECK(px(grab(w), W / 2, H / 2) == QColor(255, 0, 0));

      w.setSourceRect({0.5, 0.0, 0.5, 1.0});
      CHECK(px(grab(w), W / 2, H / 2) == QColor(0, 255, 0));
    }
  });
}

TEST_CASE("PreviewWidget blend gradients", "[gfx][window][preview]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    // Output 0's identification colour is a flat fill, so the overlay's effect
    // is readable analytically: the overlay alpha is 1 - pow(t, gamma) over
    // black, hence the result is base * pow(t, gamma), t = 0 at the edge.
    const QColor base = idColors[0];

    SECTION("a left blend darkens the left edge with the gamma curve")
    {
      PreviewWidget w{0, PreviewContent::OutputIdentification};
      w.setBlend({0.5f, 1.0f}, {}, {}, {});
      const auto img = grab(w);

      // t = x / (0.5 * W): sample at t = 0.5.
      const int x = int(0.25 * W);
      const auto c = px(img, x, 3);
      INFO("sampled " << c.red() << "," << c.green() << "," << c.blue());
      CHECK(c.blue() == Approx(base.blue() * 0.5).margin(12));
      CHECK(c.green() == Approx(base.green() * 0.5).margin(12));

      // Past the blend width nothing is touched.
      CHECK(px(img, int(0.75 * W), 3) == base);
      // At the very edge it is (nearly) black.
      CHECK(px(img, 0, 3).blue() < base.blue() / 3);
    }

    SECTION("gamma bends the curve")
    {
      PreviewWidget linear{0, PreviewContent::OutputIdentification};
      linear.setBlend({0.5f, 1.0f}, {}, {}, {});
      const auto lin = grab(linear);

      PreviewWidget steep{0, PreviewContent::OutputIdentification};
      steep.setBlend({0.5f, 3.0f}, {}, {}, {});
      const auto st = grab(steep);

      const int x = int(0.25 * W);
      // pow(0.5, 3) = 0.125 << pow(0.5, 1) = 0.5
      CHECK(px(st, x, 3).blue() < px(lin, x, 3).blue());
      CHECK(px(st, x, 3).blue() == Approx(base.blue() * 0.125).margin(12));
    }

    SECTION("each side darkens its own edge")
    {
      PreviewWidget left{0, PreviewContent::OutputIdentification};
      left.setBlend({0.25f, 1.0f}, {}, {}, {});
      const auto l = grab(left);
      CHECK(px(l, 1, H / 2).blue() < base.blue() / 3);
      CHECK(px(l, W - 2, H / 2) == base);

      PreviewWidget right{0, PreviewContent::OutputIdentification};
      right.setBlend({}, {0.25f, 1.0f}, {}, {});
      const auto r = grab(right);
      CHECK(px(r, W - 2, H / 2).blue() < base.blue() / 3);
      CHECK(px(r, 1, H / 2) == base);

      PreviewWidget top{0, PreviewContent::OutputIdentification};
      top.setBlend({}, {}, {0.25f, 1.0f}, {});
      const auto t = grab(top);
      CHECK(px(t, 3, 1).blue() < base.blue() / 3);
      CHECK(px(t, 3, H - 2) == base);

      PreviewWidget bottom{0, PreviewContent::OutputIdentification};
      bottom.setBlend({}, {}, {}, {0.25f, 1.0f});
      const auto b = grab(bottom);
      CHECK(px(b, 3, H - 2).blue() < base.blue() / 3);
      CHECK(px(b, 3, 1) == base);
    }

    SECTION("a zero-width blend paints nothing")
    {
      PreviewWidget none{0, PreviewContent::OutputIdentification};
      const auto a = grab(none);
      PreviewWidget zero{0, PreviewContent::OutputIdentification};
      zero.setBlend({0.f, 1.f}, {0.f, 1.f}, {0.f, 1.f}, {0.f, 1.f});
      const auto b = grab(zero);
      CHECK(differingPixels(a, b) == 0);
    }
  });
}

TEST_CASE("PreviewWidget warp and transform", "[gfx][window][preview]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    QImage card{64, 64, QImage::Format_ARGB32};
    for(int y = 0; y < 64; ++y)
      for(int x = 0; x < 64; ++x)
        card.setPixelColor(x, y, y < 32 ? QColor(255, 0, 0) : QColor(0, 255, 0));

    auto make = [&] {
      auto* w = new PreviewWidget{0, PreviewContent::GlobalTestCard};
      w->setGlobalTestCard(card);
      return w;
    };

    SECTION("an identity warp changes nothing")
    {
      auto ref = make();
      const auto a = grab(*ref);
      auto id = make();
      id->setCornerWarp(CornerWarp{});
      const auto b = grab(*id);
      CHECK(differingPixels(a, b) == 0);
      delete ref;
      delete id;
    }

    SECTION("a corner warp moves pixels")
    {
      auto ref = make();
      const auto a = grab(*ref);
      auto warped = make();
      CornerWarp cw;
      cw.topLeft = {0.25, 0.15};
      warped->setCornerWarp(cw);
      const auto b = grab(*warped);
      CHECK(differingPixels(a, b) > (W * H) / 20);
      delete ref;
      delete warped;
    }

    SECTION("180 degrees swaps top and bottom")
    {
      auto plain = make();
      const auto a = grab(*plain);
      CHECK(px(a, W / 2, 8).red() > 200);
      CHECK(px(a, W / 2, H - 8).green() > 200);

      auto rot = make();
      rot->setTransform(180, false, false);
      const auto b = grab(*rot);
      CHECK(px(b, W / 2, 8).green() > 200);
      CHECK(px(b, W / 2, H - 8).red() > 200);
      delete plain;
      delete rot;
    }

    SECTION("mirrorY flips vertically, mirrorX does not")
    {
      auto my = make();
      my->setTransform(0, false, true);
      const auto b = grab(*my);
      CHECK(px(b, W / 2, 8).green() > 200);
      CHECK(px(b, W / 2, H - 8).red() > 200);

      auto mx = make();
      mx->setTransform(0, true, false);
      const auto c = grab(*mx);
      // The card is a horizontal split, so a horizontal mirror leaves the
      // vertical arrangement alone.
      CHECK(px(c, W / 2, 8).red() > 200);
      CHECK(px(c, W / 2, H - 8).green() > 200);
      delete my;
      delete mx;
    }

    SECTION("a no-op transform is not applied at all")
    {
      auto ref = make();
      const auto a = grab(*ref);
      auto same = make();
      same->setTransform(0, false, false);
      const auto b = grab(*same);
      CHECK(differingPixels(a, b) == 0);
      delete ref;
      delete same;
    }
  });
}

TEST_CASE("PreviewWidget fullscreen toggle", "[gfx][window][preview]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    PreviewWidget w{3, PreviewContent::Black};
    w.resize(W, H);

    int calls{};
    int lastIndex{-1};
    bool lastState{};
    w.onFullscreenToggled = [&](int i, bool fs) {
      ++calls;
      lastIndex = i;
      lastState = fs;
    };

    QMouseEvent dbl{QEvent::MouseButtonDblClick, QPointF{5, 5},   QPointF{5, 5},
                    Qt::LeftButton,              Qt::LeftButton,  Qt::NoModifier};
    QApplication::sendEvent(&w, &dbl);
    CHECK(calls == 1);
    CHECK(lastIndex == 3);
    CHECK(lastState);
    CHECK(w.isFullScreen());

    QApplication::sendEvent(&w, &dbl);
    CHECK(calls == 2);
    CHECK_FALSE(lastState);
    CHECK_FALSE(w.isFullScreen());

    // The index the callback reports follows setOutputIndex.
    w.setOutputIndex(6);
    QApplication::sendEvent(&w, &dbl);
    CHECK(lastIndex == 6);
    CHECK(w.windowTitle() == QStringLiteral("Output 6"));
  });
}

TEST_CASE("OutputPreviewWindows tracks the mapping list", "[gfx][window][preview]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext&) {
    OutputPreviewWindows p;

    auto mapping = [](QPoint pos, QSize sz) {
      OutputMapping m;
      m.windowPosition = pos;
      m.windowSize = sz;
      return m;
    };

    SECTION("windows are created, resized and removed to match")
    {
      p.syncToMappings({mapping({10, 10}, {160, 120})});
      auto tops = QApplication::topLevelWidgets();
      int previews = 0;
      for(auto* w : tops)
        if(dynamic_cast<PreviewWidget*>(w))
          ++previews;
      CHECK(previews == 1);

      p.syncToMappings(
          {mapping({10, 10}, {160, 120}), mapping({200, 10}, {160, 120}),
           mapping({400, 10}, {160, 120})});
      previews = 0;
      for(auto* w : QApplication::topLevelWidgets())
        if(auto* pw = dynamic_cast<PreviewWidget*>(w))
          if(pw->isVisible())
            ++previews;
      CHECK(previews == 3);

      p.syncToMappings({mapping({10, 10}, {160, 120})});
      previews = 0;
      for(auto* w : QApplication::topLevelWidgets())
        if(auto* pw = dynamic_cast<PreviewWidget*>(w))
          if(pw->isVisible())
            ++previews;
      CHECK(previews == 1);

      p.syncToMappings({});
      previews = 0;
      for(auto* w : QApplication::topLevelWidgets())
        if(auto* pw = dynamic_cast<PreviewWidget*>(w))
          if(pw->isVisible())
            ++previews;
      CHECK(previews == 0);
    }

    SECTION("geometry follows the mapping when positions are synced")
    {
      p.syncToMappings({mapping({40, 50}, {200, 150})});
      PreviewWidget* pw{};
      for(auto* w : QApplication::topLevelWidgets())
        if(auto* c = dynamic_cast<PreviewWidget*>(w))
          pw = c;
      REQUIRE(pw != nullptr);
      CHECK(pw->size() == QSize{200, 150});

      // With syncing off the widget keeps whatever geometry it had.
      p.setSyncPositions(false);
      p.syncToMappings({mapping({300, 300}, {320, 240})});
      CHECK(pw->size() == QSize{200, 150});

      p.setSyncPositions(true);
      p.syncToMappings({mapping({300, 300}, {320, 240})});
      CHECK(pw->size() == QSize{320, 240});
    }

    SECTION("a fullscreen mapping goes fullscreen and back")
    {
      auto m = mapping({40, 50}, {200, 150});
      m.fullscreen = true;
      m.screenIndex = 0;
      p.syncToMappings({m});

      PreviewWidget* pw{};
      for(auto* w : QApplication::topLevelWidgets())
        if(auto* c = dynamic_cast<PreviewWidget*>(w))
          pw = c;
      REQUIRE(pw != nullptr);
      CHECK(pw->isFullScreen());

      m.fullscreen = false;
      p.syncToMappings({m});
      CHECK_FALSE(pw->isFullScreen());
    }

    SECTION("an out-of-range screen index is ignored")
    {
      auto m = mapping({40, 50}, {200, 150});
      m.screenIndex = 99;
      p.syncToMappings({m});
      PreviewWidget* pw{};
      for(auto* w : QApplication::topLevelWidgets())
        if(auto* c = dynamic_cast<PreviewWidget*>(w))
          pw = c;
      REQUIRE(pw != nullptr);
      CHECK(pw->size() == QSize{200, 150});
    }

    SECTION("the fullscreen callback is forwarded")
    {
      int idx{-1};
      bool state{};
      p.onFullscreenToggled = [&](int i, bool fs) {
        idx = i;
        state = fs;
      };
      p.syncToMappings({mapping({10, 10}, {160, 120})});
      PreviewWidget* pw{};
      for(auto* w : QApplication::topLevelWidgets())
        if(auto* c = dynamic_cast<PreviewWidget*>(w))
          pw = c;
      REQUIRE(pw != nullptr);

      QMouseEvent dbl{QEvent::MouseButtonDblClick, QPointF{5, 5},   QPointF{5, 5},
                      Qt::LeftButton,              Qt::LeftButton,  Qt::NoModifier};
      QApplication::sendEvent(pw, &dbl);
      CHECK(idx == 0);
      CHECK(state);
    }

    SECTION("the content mode reaches every window")
    {
      p.syncToMappings({mapping({10, 10}, {160, 120}), mapping({200, 10}, {160, 120})});
      p.setPreviewContent(PreviewContent::OutputIdentification);

      std::vector<PreviewWidget*> pws;
      for(auto* w : QApplication::topLevelWidgets())
        if(auto* c = dynamic_cast<PreviewWidget*>(w))
          if(c->isVisible())
            pws.push_back(c);
      REQUIRE(pws.size() == 2);
      for(auto* pw : pws)
      {
        pw->setFixedSize(W, H);
        const auto img = pw->grab().toImage();
        CHECK_FALSE(isBlack(img.pixelColor(3, 3)));
      }
    }

    SECTION("the input resolution rebuilds the shared test card")
    {
      p.setPreviewContent(PreviewContent::GlobalTestCard);
      p.syncToMappings({mapping({10, 10}, {160, 120})});
      PreviewWidget* pw{};
      for(auto* w : QApplication::topLevelWidgets())
        if(auto* c = dynamic_cast<PreviewWidget*>(w))
          pw = c;
      REQUIRE(pw != nullptr);
      pw->setFixedSize(W, H);
      const auto before = pw->grab().toImage();
      CHECK(meanLuma(before.convertToFormat(QImage::Format_ARGB32)) > 5.0);

      // A different aspect ratio changes the card, hence the preview.
      p.setInputResolution({640, 640});
      p.syncToMappings({mapping({10, 10}, {160, 120})});
      const auto after = pw->grab().toImage();
      CHECK(differingPixels(
                before.convertToFormat(QImage::Format_ARGB32),
                after.convertToFormat(QImage::Format_ARGB32))
            > 0);

      // Re-setting the same resolution is a no-op.
      p.setInputResolution({640, 640});
      const auto again = pw->grab().toImage();
      CHECK(differingPixels(
                after.convertToFormat(QImage::Format_ARGB32),
                again.convertToFormat(QImage::Format_ARGB32))
            == 0);
    }
  });
}
