// The knob-like controls paint nothing outside their bounding rect, at every
// size and device pixel ratio: a view only repaints that rect, anything past
// it is left behind as a trail.
//
// The hidden case prints the cost of one paint() into a raster image:
//
//   tests/unit/test_unit_graphics_knob_paint "[.knob_paint_bench]"

#include <score_test/App.hpp>

#include <score/graphics/WidgetPresentation.hpp>
#include <score/graphics/widgets/QGraphicsKnob.hpp>
#include <score/graphics/widgets/QGraphicsLogKnob.hpp>
#include <score/graphics/widgets/QGraphicsTimeChooser.hpp>
#include <score/model/Skin.hpp>
#include <score/serialization/StringConstants.hpp>

#include <QElapsedTimer>
#include <QImage>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdio>

namespace
{
//! Pixels painted outside the item's bounding rect.
int strayPixels(QGraphicsItem& item, qreal dpr, qreal zoom)
{
  const QRectF br = item.boundingRect();
  constexpr double margin = 8.;
  const QSizeF logical = (br.size() + QSizeF{2 * margin, 2 * margin}) * zoom;
  QImage img{
      int(std::ceil(logical.width() * dpr)), int(std::ceil(logical.height() * dpr)),
      QImage::Format_ARGB32_Premultiplied};
  img.setDevicePixelRatio(dpr);
  img.fill(Qt::transparent);
  {
    QPainter p{&img};
    p.scale(zoom, zoom);
    p.translate(margin - br.x(), margin - br.y());
    QStyleOptionGraphicsItem opt;
    opt.state |= QStyle::State_MouseOver;
    item.paint(&p, &opt, nullptr);
  }
  // The bounding rect, in device pixels
  const QRectF dev{
      margin * zoom * dpr, margin * zoom * dpr, br.width() * zoom * dpr,
      br.height() * zoom * dpr};
  int stray = 0;
  for(int y = 0; y < img.height(); y++)
    for(int x = 0; x < img.width(); x++)
      if(qAlpha(img.pixel(x, y)) != 0 && !dev.intersects(QRectF(x, y, 1, 1)))
        stray++;
  return stray;
}

template <typename F>
double usPerCall(const char* name, qreal dpr, F&& f)
{
  QImage img{int(80 * dpr), int(80 * dpr), QImage::Format_ARGB32_Premultiplied};
  img.setDevicePixelRatio(dpr);
  img.fill(Qt::transparent);
  QPainter p{&img};
  for(int i = 0; i < 200; i++)
    f(p, i);
  constexpr int N = 20000;
  QElapsedTimer t;
  t.start();
  for(int i = 0; i < N; i++)
    f(p, i);
  const double us = t.nsecsElapsed() / 1000. / N;
  std::printf("%-40s dpr %.0f: %7.2f us\n", name, dpr, us);
  return us;
}
}

TEST_CASE("knob-like controls paint inside their bounding rect", "[knob]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    for(qreal dpr : {1., 1.5, 2.})
      for(qreal zoom : {0.5, 1., 2.})
        for(auto size :
            {score::ControlSize::Normal, score::ControlSize::Small,
             score::ControlSize::Large})
          for(double v : {0., 0.5, 1.})
          {
            INFO("dpr " << dpr << ", zoom " << zoom << ", size " << int(size)
                        << ", value " << v);
            score::QGraphicsKnob knob{nullptr};
            knob.setRange(-1000., 1000., 0.);
            score::setControlSize(knob, size);
            knob.setValue(v);
            CHECK(strayPixels(knob, dpr, zoom) == 0);

            score::QGraphicsLogKnob logKnob{nullptr};
            logKnob.setRange(20., 20000., 440.);
            score::setControlSize(logKnob, size);
            logKnob.setValue(v);
            CHECK(strayPixels(logKnob, dpr, zoom) == 0);
          }

    for(qreal dpr : {1., 2.})
      for(qreal zoom : {0.5, 1., 2.})
      {
        INFO("dpr " << dpr << ", zoom " << zoom);
        score::QGraphicsTimeChooser t{nullptr};
        t.setRange(0., 60., 0.);
        for(float v : {0.f, 0.5f, 1.f})
        {
          t.setValue({v, 0.f});
          INFO(t.freeText().toStdString());
          CHECK(strayPixels(t, dpr, zoom) == 0);
        }
        for(float v : {1.f / 64.f, 3.f / 8.f, 4.f})
        {
          t.setValue({v, 1.f});
          CHECK(strayPixels(t, dpr, zoom) == 0);
        }
      }
  });
}

TEST_CASE("paint cost of the knob-like controls", "[.knob_paint_bench]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto& skin = score::Skin::instance();
    QStyleOptionGraphicsItem opt;
    for(qreal dpr : {1., 2.})
    {
      score::QGraphicsKnob knob{nullptr};
      knob.setRange(0., 1., 0.);
      usPerCall("knob", dpr, [&](QPainter& p, int i) {
        knob.setValue((i % 100) / 100.);
        static_cast<QGraphicsItem&>(knob).paint(&p, &opt, nullptr);
      });

      score::QGraphicsTimeChooser free{nullptr};
      free.setRange(0., 5., 0.);
      usPerCall("time chooser, free", dpr, [&](QPainter& p, int i) {
        free.setValue({(i % 100) / 100.f, 0.f});
        static_cast<QGraphicsItem&>(free).paint(&p, &opt, nullptr);
      });

      score::QGraphicsTimeChooser sync{nullptr};
      usPerCall("time chooser, synced", dpr, [&](QPainter& p, int i) {
        sync.setValue({(i % 2) ? 0.25f : 0.125f, 1.f});
        static_cast<QGraphicsItem&>(sync).paint(&p, &opt, nullptr);
      });

      // The knob as it was drawn before the 35 px reference design
      usPerCall("  previous knob design", dpr, [&](QPainter& p, int i) {
        const double v = (i % 100) / 100.;
        constexpr double start = 220. * 16., totalSpan = 260. * 16.;
        const QRectF r{6., 6., 23., 23.};
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(skin.Emphasis2.main.pen1);
        p.setBrush(skin.Emphasis2.main.brush);
        p.drawChord(r, start, -totalSpan);
        p.setPen(skin.Base4.main.pen2_solid_round_round);
        p.drawArc(r, start, -v * totalSpan);
        const double theta = -0.0174533 * (start - v * totalSpan) / 16.;
        p.drawLine(r.center(), r.center() + 11.5 * QPointF{std::cos(theta), std::sin(theta)});
        p.setPen(skin.Base4.lighter180.pen1);
        p.setFont(skin.Medium8Pt);
        p.drawText(QRectF{0., 25., 35., 10.}, score::toNumber(v), QTextOption(Qt::AlignCenter));
        p.setRenderHint(QPainter::Antialiasing, false);
      });

      const QString text = QStringLiteral("0.50");
      usPerCall("  value text, drawText", dpr, [&](QPainter& p, int) {
        p.setFont(skin.Medium8Pt);
        p.drawText(QRectF{0., 35., 35., 10.}, text, QTextOption(Qt::AlignCenter));
      });

      usPerCall("  dial only (no text)", dpr, [&](QPainter& p, int i) {
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(skin.Emphasis2.main.brush);
        p.drawChord(QRectF{6., 6., 23., 23.}, 220 * 16, -260 * 16);
        p.setBrush(Qt::NoBrush);
        p.setPen(skin.Base4.main.pen2_solid_round_round);
        p.drawArc(QRectF{7., 7., 21., 21.}, 220 * 16, -(i % 100) * 26 * 16 / 10);
        p.drawLine(QPointF{17.5, 17.5}, QPointF{27., 20.});
      });

      QString log;
      for(int i = 0; i < 32; i++)
        log += QStringLiteral("%1\n").arg(i * 0.123456);
      usPerCall("  value display, 32 lines, drawText", dpr, [&](QPainter& p, int) {
        p.setFont(skin.MonoFontSmall);
        p.drawText(QRectF{10., 0., 70., 80.}, log);
      });
    }
  });
}
