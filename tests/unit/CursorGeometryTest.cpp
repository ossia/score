// The size and hotspot score gives its cursors, checked against what each
// platform plug-in of Qt 6.12 does with a pixmap cursor. The plug-ins are
// modelled here from their sources; only X11 can be seen on this machine.
#include <score/widgets/CursorTheme.hpp>

#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QSizeF>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using score::CursorPlatform;

namespace
{
struct OnScreen
{
  //! In device pixels.
  QSizeF size;
  QPointF hotspot;
  //! The platform resamples the pixmap.
  bool resampled{};
};

//! What Qt and the system show for a QCursor built from a pixmap of
//! g.pixelSize pixels, device pixel ratio g.devicePixelRatio, hotspot
//! g.hotspot, on a screen whose scale (without the zoom) is screenScale.
OnScreen platformShows(
    CursorPlatform p, const score::CursorGeometry& g, double zoom, double screenScale)
{
  const QSizeF px{g.pixelSize};
  switch(p)
  {
    case CursorPlatform::X11:
      // qxcbcursor.cpp createBitmapCursor: the image and hotSpot() go to
      // xcb_render_create_cursor as they are.
      return {px, QPointF(g.hotspot), false};

    case CursorPlatform::Windows: {
      // qwindowscursor.cpp pixmapWindowCursor + createPixmapCursor:
      // scaleFactor = QHighDpiScaling::factor(screen) = zoom * screenScale.
      const double factor = zoom * screenScale;
      const double pixmapScaleFactor = factor / g.devicePixelRatio;
      const bool resample = !qFuzzyCompare(pixmapScaleFactor, 1.);
      const QSizeF size = resample ? QSizeF((pixmapScaleFactor * px).toSize()) : px;
      return {
          size,
          QPointF(std::round(g.hotspot.x() * factor), std::round(g.hotspot.y() * factor)),
          resample};
    }

    case CursorPlatform::MacOS: {
      // qcocoacursor.mm createCursorFromPixmap + NSImage imageFromQImage:
      // the NSImage measures deviceIndependentSize() points, the hotspot is
      // in points. The window server draws points at the backing scale.
      const QSizeF points = px / g.devicePixelRatio;
      const double backing = screenScale;
      return {
          points * backing, QPointF(g.hotspot) * backing,
          !qFuzzyCompare(g.devicePixelRatio, backing)};
    }

    case CursorPlatform::Wayland: {
      // qwaylandinputdevice.cpp setCursor: bitmapScale = int(dpr), and the
      // hotspot is multiplied by dpr / bitmapScale when dpr is fractional;
      // the surface measures pixels / bitmapScale in compositor coordinates,
      // which the compositor draws at the output's scale.
      const int bitmapScale = int(g.devicePixelRatio);
      QPointF hot = QPointF(g.hotspot);
      if(bitmapScale < g.devicePixelRatio)
        hot *= g.devicePixelRatio / bitmapScale;
      const QSizeF surface = px / bitmapScale;
      return {
          surface * screenScale, hot * screenScale,
          !qFuzzyCompare(double(bitmapScale), screenScale)};
    }
  }
  return {};
}

constexpr double zooms[] = {0.5, 1., 1.25, 1.5, 1.75, 2., 3.};
}

TEST_CASE("A cursor measures its canvas times the zoom times the screen scale", "[cursor]")
{
  struct Case
  {
    CursorPlatform platform;
    std::initializer_list<double> screenScales;
  };
  // X11's screen scale is the desktop pointer size over 24; Windows has
  // fractional scales per screen; macOS 1 and 2; Wayland outputs with integer
  // scales here, fractional ones below.
  const Case cases[] = {
      {CursorPlatform::X11, {1., 22. / 24., 1.5, 2.}},
      {CursorPlatform::Windows, {1., 1.25, 1.5, 1.75, 2., 3.}},
      {CursorPlatform::MacOS, {1., 2.}},
      {CursorPlatform::Wayland, {1., 2., 3.}},
  };

  for(const auto& c : cases)
  {
    for(double s : c.screenScales)
    {
      for(double z : zooms)
      {
        for(int ai = 0; ai < int(score::CursorArtwork::Count); ai++)
        {
          const auto a = score::CursorArtwork(ai);
          const auto& info = score::cursorArtworkInfo(a);
          const auto g = score::cursorGeometry(c.platform, info.size, info.hotspot, z, s);
          const auto shown = platformShows(c.platform, g, z, s);
          CAPTURE(int(c.platform), s, z, ai, g.pixelSize.width(), g.devicePixelRatio,
                  g.hotspot.x(), g.hotspot.y(), g.renderOffset.x(), g.renderOffset.y());

          // Within a device pixel of the exact size, and drawn pixel for
          // pixel: the platform does not resample score's rendering.
          CHECK(shown.size.width() == Catch::Approx(info.size.width() * z * s).margin(1.));
          CHECK(shown.size.height() == Catch::Approx(info.size.height() * z * s).margin(1.));
          CHECK_FALSE(shown.resampled);

          // The platform clicks the pixel the geometry says...
          CHECK(shown.hotspot.x() == g.pixelHotspot.x());
          CHECK(shown.hotspot.y() == g.pixelHotspot.y());

          // ... which is the one the artwork's hotspot point falls in once
          // moved by the render offset, a whole number of pixels, at most
          // half of the unit the platform takes the hotspot in.
          const double k = z * s;
          const QPointF drawn = info.hotspot * k + QPointF(g.renderOffset);
          CHECK(int(std::floor(drawn.x() + 1e-6)) == g.pixelHotspot.x());
          CHECK(int(std::floor(drawn.y() + 1e-6)) == g.pixelHotspot.y());
          const double unit = c.platform == CursorPlatform::X11       ? 1.
                              : c.platform == CursorPlatform::Windows ? k
                                                                      : s;
          CHECK(std::abs(g.renderOffset.x()) <= std::ceil(unit / 2.));
          CHECK(std::abs(g.renderOffset.y()) <= std::ceil(unit / 2.));
          if(c.platform == CursorPlatform::X11)
            CHECK(g.renderOffset == QPoint{});
        }
      }
    }
  }
}

TEST_CASE("Every standard shape but the blank one has score's artwork", "[cursor]")
{
  for(int s = Qt::ArrowCursor; s <= Qt::LastCursor; s++)
  {
    const auto shape = Qt::CursorShape(s);
    CAPTURE(s);
    if(shape == Qt::BlankCursor)
    {
      CHECK_FALSE(score::isThemeCursorShape(shape));
      CHECK(score::cursorArtworkForShape(shape) == score::CursorArtwork::Count);
      continue;
    }
    CHECK(score::isThemeCursorShape(shape));
    CHECK(score::cursorArtworkForShape(shape) != score::CursorArtwork::Count);
  }
  CHECK_FALSE(score::isThemeCursorShape(Qt::BitmapCursor));
  CHECK_FALSE(score::isThemeCursorShape(Qt::CustomCursor));
  CHECK(score::cursorArtworkForShape(Qt::BitmapCursor) == score::CursorArtwork::Count);

  for(int a = 0; a < int(score::CursorArtwork::Count); a++)
  {
    const auto& info = score::cursorArtworkInfo(score::CursorArtwork(a));
    CAPTURE(a);
    REQUIRE(info.svg);
    CHECK(QRectF{QPointF{}, QSizeF{info.size}}.contains(info.hotspot));
  }
}

namespace
{
using A = score::CursorArtwork;

QRect drawnRect(const QImage& img, int threshold)
{
  QRect r;
  for(int y = 0; y < img.height(); y++)
    for(int x = 0; x < img.width(); x++)
      if(qAlpha(img.pixel(x, y)) > threshold)
        r |= QRect{x, y, 1, 1};
  return r;
}

constexpr double pixel_scales[] = {1., 1.25, 1.5, 2., 3.};

//! QPixmap needs an application; nothing here depends on its platform.
void ensureApplication()
{
  if(qGuiApp)
    return;
  static int argc = 3;
  static char arg0[] = "test", arg1[] = "-platform", arg2[] = "offscreen";
  static char* argv[] = {arg0, arg1, arg2, nullptr};
  static QGuiApplication app{argc, argv};
}
}

TEST_CASE("The hotspot of each cursor is on its artwork's tip or centre", "[cursor]")
{
  // The apex of every arrow is at (10.5, 7.5) on the canvas, the centre of
  // the round join of its outline: the pixel (10, 7) at a scale of 1.
  for(A a : {A::Pointer, A::WhatsThis, A::Busy, A::DragCopy, A::DragMove, A::DragLink,
             A::PlayFromHere, A::CreationMode})
    CHECK(score::cursorArtworkInfo(a).hotspot == QPointF(10.5, 7.5));
  {
    const auto& info = score::cursorArtworkInfo(A::Pointer);
    const auto g = score::cursorGeometry(CursorPlatform::X11, info.size, info.hotspot, 1., 1.);
    CHECK(g.hotspot == QPoint(10, 7));
  }

  for(auto platform : {CursorPlatform::X11, CursorPlatform::Windows})
  {
    for(double k : pixel_scales)
    {
      for(int ai = 0; ai < int(A::Count); ai++)
      {
        const A a = A(ai);
        const auto& info = score::cursorArtworkInfo(a);
        const auto g = score::cursorGeometry(platform, info.size, info.hotspot, 1., k);
        const QImage img = score::renderCursorArtwork(a, g).convertToFormat(QImage::Format_ARGB32);
        REQUIRE_FALSE(img.isNull());
        const QPoint hot = g.pixelHotspot;
        // The outline is half a canvas unit on each side of the shapes, one
        // unit for the up arrow.
        const int outline = int(std::ceil((a == A::UpArrow ? 1. : 0.5) * k));
        const QRect drawn = drawnRect(img, 128);
        const QPointF exact = info.hotspot * k + QPointF(g.renderOffset);
        CAPTURE(int(platform), k, ai, hot.x(), hot.y(), drawn.left(), drawn.top(),
                drawn.right(), drawn.bottom());
        REQUIRE(img.rect().contains(hot));

        switch(a)
        {
          case A::Pointer:
          case A::WhatsThis:
          case A::Busy:
          case A::DragCopy:
          case A::DragMove:
          case A::DragLink:
          case A::PlayFromHere:
          case A::CreationMode:
          case A::UpArrow:
          case A::PointingHand:
            // A tip: drawn, and nothing above it, nor to the left of the
            // arrows' apex, but its outline.
            CHECK(qAlpha(img.pixel(hot)) > 100);
            CHECK(drawn.top() >= hot.y() - outline);
            CHECK(drawn.top() <= hot.y());
            if(a != A::UpArrow && a != A::PointingHand)
            {
              CHECK(drawn.left() >= hot.x() - outline);
              CHECK(drawn.left() <= hot.x());
            }
            else
            {
              // The tip is the middle of the top of the drawing.
              int left = img.width(), right = -1;
              for(int x = 0; x < img.width(); x++)
                if(qAlpha(img.pixel(x, drawn.top())) > 128)
                {
                  left = std::min(left, x);
                  right = std::max(right, x);
                }
              CHECK(std::abs((left + right + 1) / 2. - exact.x()) <= 0.5 * k + 1.);
            }
            break;

          case A::Magnifier: {
            // The centre of the lens, which is clear: as far from its rim on
            // each side.
            CHECK(qAlpha(img.pixel(hot)) < 128);
            const auto ringAt = [&](int dx, int dy) {
              QPoint p = hot;
              while(img.rect().contains(p) && qAlpha(img.pixel(p)) < 128)
                p += QPoint{dx, dy};
              return (p - hot).manhattanLength();
            };
            CHECK(std::abs(ringAt(-1, 0) - ringAt(1, 0)) <= 1 + int(k / 2));
            CHECK(std::abs(ringAt(0, -1) - ringAt(0, 1)) <= 1 + int(k / 2));
            break;
          }

          case A::OpenHand:
          case A::ClosedHand:
            // On the palm, the same point for both so that the hand stays put
            // when it closes.
            CHECK(qAlpha(img.pixel(hot)) > 100);
            CHECK(info.hotspot == score::cursorArtworkInfo(A::OpenHand).hotspot);
            break;

          default: {
            // The centre of a symmetric drawing.
            const QPointF middle = QRectF(drawn).center();
            CHECK(std::abs(middle.x() - exact.x()) <= 0.5 * k + 1.);
            CHECK(std::abs(middle.y() - exact.y()) <= 0.5 * k + 1.);
            if(a != A::SplitH && a != A::SplitV)
              CHECK(qAlpha(img.pixel(hot)) > 100);
            break;
          }
        }
      }
    }
  }
}

TEST_CASE("Drag cursors are drawn for the screen the drag is on", "[cursor]")
{
  ensureApplication();
  for(double s : pixel_scales)
  {
    CAPTURE(s);
    {
      // QWindowsDrag: shown pixel for pixel (createPixmapCursor with a
      // factor of 1 over the pixmap's ratio), the pointer at the top left.
      // The arrow then measures what the other cursors' does on that screen.
      const auto& info = score::cursorArtworkInfo(A::DragMove);
      const auto g = score::cursorGeometry(CursorPlatform::Windows, info.size, info.hotspot, 1., s);
      const QPixmap pm = score::renderDragCursor(A::DragMove, g, true);
      REQUIRE_FALSE(pm.isNull());
      CHECK(pm.devicePixelRatio() == 1.);
      const QImage art
          = score::renderCursorArtwork(A::DragMove, g).convertToFormat(QImage::Format_ARGB32);
      const QRect drawn = drawnRect(art, 0);
      CHECK(pm.size() == drawn.size());
      CHECK(pm.height() == Catch::Approx((30 - 7) * s).margin(2.));
      // The apex is at the pointer: the first pixels are its outline.
      const QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
      const int outline = int(std::ceil(0.5 * s)) + 1;
      CHECK((g.pixelHotspot - drawn.topLeft()).x() <= outline);
      CHECK((g.pixelHotspot - drawn.topLeft()).y() <= outline);
      CHECK(qAlpha(img.pixel(g.pixelHotspot - drawn.topLeft())) > 100);

      // The forbidden sign whole, down and right of the pointer.
      const auto& f = score::cursorArtworkInfo(A::Forbidden);
      const auto gf = score::cursorGeometry(CursorPlatform::Windows, f.size, f.hotspot, 1., s);
      const QPixmap pf = score::renderDragCursor(A::Forbidden, gf, true);
      CHECK(pf.width() == Catch::Approx(20 * s).margin(2.));
    }
    for(auto platform : {CursorPlatform::X11, CursorPlatform::Wayland})
    {
      if(platform == CursorPlatform::Wayland && s != std::round(s))
        continue;
      // QBasicDrag: QCursor(pixmap) puts the hotspot at width / 2 / ratio
      // device independent pixels, which must be the artwork's.
      const auto& info = score::cursorArtworkInfo(A::DragCopy);
      const auto g = score::cursorGeometry(platform, info.size, info.hotspot, 1., s);
      const QPixmap pm = score::renderDragCursor(A::DragCopy, g, false);
      REQUIRE_FALSE(pm.isNull());
      CHECK(pm.devicePixelRatio() == g.devicePixelRatio);
      const int dpr = int(g.devicePixelRatio);
      const int hotDip = int(pm.width() / 2 / pm.devicePixelRatio());
      CHECK(hotDip * dpr == pm.width() / 2);
      const QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
      const QImage art
          = score::renderCursorArtwork(A::DragCopy, g).convertToFormat(QImage::Format_ARGB32);
      CHECK(img.pixel(pm.width() / 2, pm.height() / 2) == art.pixel(g.pixelHotspot));
    }
  }
}

TEST_CASE("Windows hit tests map to the frame cursors", "[cursor]")
{
  namespace ht = score::win32_hit;
  using score::windowsHitTestCursor;
  // Qt gives the client area of a window its cursors; a blocked window gets
  // none from Qt.
  CHECK(windowsHitTestCursor(ht::Client, false) == A::Count);
  CHECK(windowsHitTestCursor(ht::Client, true) == A::Pointer);
  for(bool blocked : {false, true})
  {
    CAPTURE(blocked);
    for(int h : {ht::Error, ht::Caption, ht::SysMenu, ht::MinButton, ht::MaxButton,
                 ht::Close, ht::Help, ht::Border, ht::Menu, ht::HScroll, ht::VScroll})
      CHECK(windowsHitTestCursor(h, blocked) == A::Pointer);
    CHECK(windowsHitTestCursor(ht::Left, blocked) == A::ScaleH);
    CHECK(windowsHitTestCursor(ht::Right, blocked) == A::ScaleH);
    CHECK(windowsHitTestCursor(ht::Top, blocked) == A::ScaleV);
    CHECK(windowsHitTestCursor(ht::Bottom, blocked) == A::ScaleV);
    CHECK(windowsHitTestCursor(ht::TopLeft, blocked) == A::ScaleFDiag);
    CHECK(windowsHitTestCursor(ht::BottomRight, blocked) == A::ScaleFDiag);
    CHECK(windowsHitTestCursor(ht::GrowBox, blocked) == A::ScaleFDiag);
    CHECK(windowsHitTestCursor(ht::TopRight, blocked) == A::ScaleBDiag);
    CHECK(windowsHitTestCursor(ht::BottomLeft, blocked) == A::ScaleBDiag);
    for(int h : {ht::Nowhere, ht::Transparent, ht::Object, 22, 1000})
      CHECK(windowsHitTestCursor(h, blocked) == A::Count);
  }
  // The hit test arrives as the low word of lParam.
  CHECK(int(short(std::uint16_t(0xFFFE))) == ht::Error);

  // The artworks the frame gets: the shapes Qt maps the same sides to.
  CHECK(score::cursorArtworkForShape(Qt::SizeFDiagCursor) == A::ScaleFDiag);
  CHECK(score::cursorArtworkForShape(Qt::SizeBDiagCursor) == A::ScaleBDiag);
  CHECK(score::cursorArtworkForShape(Qt::SizeHorCursor) == A::ScaleH);
  CHECK(score::cursorArtworkForShape(Qt::SizeVerCursor) == A::ScaleV);
}

TEST_CASE("The platform is told apart by the name of Qt's plug-in", "[cursor]")
{
  CHECK(score::cursorPlatform("xcb") == CursorPlatform::X11);
  CHECK(score::cursorPlatform("windows") == CursorPlatform::Windows);
  CHECK(score::cursorPlatform("cocoa") == CursorPlatform::MacOS);
  CHECK(score::cursorPlatform("wayland") == CursorPlatform::Wayland);
  CHECK(score::cursorPlatform("wayland-egl") == CursorPlatform::Wayland);
  CHECK(score::cursorPlatform("offscreen") == CursorPlatform::Windows);
}
