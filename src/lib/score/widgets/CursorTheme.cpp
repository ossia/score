#include "CursorTheme.hpp"

#include "CursorThemeNative.hpp"

#include <score/tools/X11CursorScale.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QApplication>
#include <QDrag>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QTextEdit>
#include <QVariant>
#include <QWidget>
#include <QWindow>

#include <cmath>
#include <vector>

#if __has_include(<QSvgRenderer>)
#include <QSvgRenderer>
#define SCORE_CURSORS_HAVE_SVG 1
#else
#define SCORE_CURSORS_HAVE_SVG 0
#endif

// The artwork is in score's resources, which a static score_lib_base links
// into a binary only when something there references them: referenced here,
// so that whatever draws cursors has them. Q_INIT_RESOURCE only works outside
// of any namespace.
static void registerCursorResources()
{
  static const bool registered = [] {
    Q_INIT_RESOURCE(score);
    return true;
  }();
  (void)registered;
}

#if QT_CONFIG(draganddrop) && __has_include(<QtGui/private/qdnd_p.h>)
#include <QtGui/private/qdnd_p.h>
#define SCORE_CURSORS_HAVE_DRAG_MANAGER 1
#else
#define SCORE_CURSORS_HAVE_DRAG_MANAGER 0
#endif

namespace score
{
namespace
{
double g_zoom = 1.;

#define SCORE_CURSOR_SVG(name) ":/icons_svg/mouse_pointer/cursor_" name ".svg"

// Where each artwork clicks, measured on its SVG: the apex of the arrow every
// arrow-shaped cursor shares is at (10.5, 7.5), the centre of the round join
// of its outline. The two hands share theirs so that the hand does not jump
// when it closes.
constexpr QPointF arrow_apex{10.5, 7.5};
constexpr QPointF centre{16., 16.};

const std::array<CursorArtworkInfo, std::size_t(CursorArtwork::Count)>& artworks()
{
  static const std::array<CursorArtworkInfo, std::size_t(CursorArtwork::Count)> a{{
      {SCORE_CURSOR_SVG("pointer"), {32, 32}, arrow_apex},
      {SCORE_CURSOR_SVG("up_arrow"), {32, 32}, {16., 4.}},
      {SCORE_CURSOR_SVG("cross"), {32, 32}, centre},
      {SCORE_CURSOR_SVG("wait"), {32, 32}, centre},
      {SCORE_CURSOR_SVG("ibeam"), {32, 32}, centre},
      // The shafts of the resize arrows are one unit thick, centred on the
      // middle of the canvas's sixteenth row or column.
      {SCORE_CURSOR_SVG("scale_v"), {32, 32}, {16.5, 16.}},
      {SCORE_CURSOR_SVG("scale_h"), {32, 32}, {16., 16.5}},
      {SCORE_CURSOR_SVG("scale_fdiag"), {32, 32}, centre, true},
      {SCORE_CURSOR_SVG("scale_fdiag"), {32, 32}, centre},
      {SCORE_CURSOR_SVG("move"), {32, 32}, {15.5, 15.5}},
      {SCORE_CURSOR_SVG("split_v"), {32, 32}, centre},
      {SCORE_CURSOR_SVG("split_h"), {32, 32}, centre},
      // The tip of the index finger.
      {SCORE_CURSOR_SVG("pointing_hand"), {32, 32}, {16.5, 17.5}},
      {SCORE_CURSOR_SVG("forbidden"), {32, 32}, centre},
      {SCORE_CURSOR_SVG("whats_this"), {32, 32}, arrow_apex},
      {SCORE_CURSOR_SVG("busy"), {32, 32}, arrow_apex},
      {SCORE_CURSOR_SVG("open_hand"), {32, 32}, centre},
      {SCORE_CURSOR_SVG("closed_hand"), {32, 32}, centre},
      {SCORE_CURSOR_SVG("drag_copy"), {32, 32}, arrow_apex},
      {SCORE_CURSOR_SVG("drag_move"), {32, 32}, arrow_apex},
      {SCORE_CURSOR_SVG("drag_link"), {32, 32}, arrow_apex},
      // The centre of the lens.
      {SCORE_CURSOR_SVG("magnifier"), {32, 32}, {15.08, 15.07}},
      {SCORE_CURSOR_SVG("spin"), {32, 32}, centre},
      {SCORE_CURSOR_SVG("play_from_here"), {32, 32}, arrow_apex},
      {SCORE_CURSOR_SVG("creation_mode"), {32, 32}, arrow_apex},
  }};
  return a;
}
#undef SCORE_CURSOR_SVG

// Indexed like Qt::CursorShape up to Qt::LastCursor.
constexpr std::array<CursorArtwork, Qt::LastCursor + 1> shape_artworks{
    CursorArtwork::Pointer,      // ArrowCursor
    CursorArtwork::UpArrow,      // UpArrowCursor
    CursorArtwork::Cross,        // CrossCursor
    CursorArtwork::Wait,         // WaitCursor
    CursorArtwork::IBeam,        // IBeamCursor
    CursorArtwork::ScaleV,       // SizeVerCursor
    CursorArtwork::ScaleH,       // SizeHorCursor
    CursorArtwork::ScaleBDiag,   // SizeBDiagCursor
    CursorArtwork::ScaleFDiag,   // SizeFDiagCursor
    CursorArtwork::Move,         // SizeAllCursor
    CursorArtwork::Count,        // BlankCursor
    CursorArtwork::SplitV,       // SplitVCursor
    CursorArtwork::SplitH,       // SplitHCursor
    CursorArtwork::PointingHand, // PointingHandCursor
    CursorArtwork::Forbidden,    // ForbiddenCursor
    CursorArtwork::WhatsThis,    // WhatsThisCursor
    CursorArtwork::Busy,         // BusyCursor
    CursorArtwork::OpenHand,     // OpenHandCursor
    CursorArtwork::ClosedHand,   // ClosedHandCursor
    CursorArtwork::DragCopy,     // DragCopyCursor
    CursorArtwork::DragMove,     // DragMoveCursor
    CursorArtwork::DragLink,     // DragLinkCursor
};
static_assert(Qt::DragLinkCursor == Qt::LastCursor);

// Order of the drag cursors of a set.
constexpr std::array<std::pair<Qt::DropAction, CursorArtwork>, 4> drag_artworks{{
    {Qt::CopyAction, CursorArtwork::DragCopy},
    {Qt::MoveAction, CursorArtwork::DragMove},
    {Qt::LinkAction, CursorArtwork::DragLink},
    {Qt::IgnoreAction, CursorArtwork::Forbidden},
}};

int dragIndex(Qt::DropAction action) noexcept
{
  switch(action)
  {
    case Qt::CopyAction:
      return 0;
    case Qt::LinkAction:
      return 2;
    case Qt::IgnoreAction:
      return 3;
    default:
      // QBasicDrag shows Qt::DragMoveCursor for every other action.
      return 1;
  }
}

int dragIndex(Qt::CursorShape shape) noexcept
{
  switch(shape)
  {
    case Qt::DragCopyCursor:
      return 0;
    case Qt::DragMoveCursor:
      return 1;
    case Qt::DragLinkCursor:
      return 2;
    case Qt::ForbiddenCursor:
      return 3;
    default:
      return -1;
  }
}

//! Scales within this of each other share their cursors.
constexpr double same_scale = 1e-4;

QPixmap toPixmap(QImage img, qreal dpr)
{
  QPixmap pm = QPixmap::fromImage(std::move(img));
  pm.setDevicePixelRatio(dpr);
  return pm;
}

//! The integer hotspot \p h, in units of \p unit pixels, whose pixel is the
//! closest to \p target once the platform rounds h * unit with \p round.
template <typename Round>
int closestUnit(int target, double unit, Round round) noexcept
{
  const int guess = int(std::lround(target / unit));
  int best = guess;
  int bestDistance = std::abs(round(guess * unit) - target);
  for(int h : {guess - 1, guess + 1})
  {
    if(h < 0)
      continue;
    if(const int d = std::abs(round(h * unit) - target); d < bestDistance)
    {
      best = h;
      bestDistance = d;
    }
  }
  return std::max(0, best);
}

//! The viewports of QTextEdit and QTextBrowser take the shape they find under
//! the hand of a link or a list marker for the one to restore after it: they
//! keep the standard hand, and their window gets score's.
bool keepsStandardShape(const QWidget& w, Qt::CursorShape shape)
{
  if(shape == Qt::PointingHandCursor)
    if(auto* edit = qobject_cast<QTextEdit*>(w.parentWidget()))
      return edit->viewport() == &w;
  return false;
}

//! A disabled widget with a cursor of its own gets its window's cursor unset
//! by Qt, which shows the platform's arrow: it gives its cursor up while it
//! is disabled, so that it shows its parent's, and takes it back after.
constexpr auto disabled_cursor_property = "_score_cursor_while_enabled";

void followEnabledState(QWidget& w)
{
  if(!w.isEnabled())
  {
    if(w.testAttribute(Qt::WA_SetCursor) && !w.isWindow()
       && w.cursor().shape() == Qt::BitmapCursor)
    {
      w.setProperty(disabled_cursor_property, QVariant::fromValue(w.cursor()));
      w.unsetCursor();
    }
  }
  else if(const QVariant v = w.property(disabled_cursor_property); v.isValid())
  {
    w.setProperty(disabled_cursor_property, {});
    if(!w.testAttribute(Qt::WA_SetCursor))
      w.setCursor(v.value<QCursor>());
  }
}

bool sameCursor(const QCursor& a, const QCursor& b)
{
  if(a.shape() != b.shape())
    return false;
  return a.shape() != Qt::BitmapCursor || a.pixmap().cacheKey() == b.pixmap().cacheKey();
}

//! QGraphicsView gives its viewport the cursor of the item under the mouse
//! at every move: the items under the mouse that have the cursor the
//! viewport had get the one it gets, so that the next moves give the
//! viewport that one directly.
//! The position of the last mouse move in the viewport, when it was delivered
//! to it: mapping the global one back is wrong on a window that spans screens
//! of different scales.
void replaceItemCursors(
    const QWidget& viewport, const QObject* lastMoveTarget, QPointF lastMoveLocal,
    QPointF globalPos, const QCursor& old, const QCursor& c)
{
  auto* view = qobject_cast<QGraphicsView*>(viewport.parentWidget());
  if(!view || view->viewport() != &viewport || !view->scene())
    return;
  const QPoint pos = (lastMoveTarget == &viewport ? lastMoveLocal
                                                   : viewport.mapFromGlobal(globalPos))
                         .toPoint();
  for(QGraphicsItem* item : view->items(pos))
    if(item->hasCursor() && sameCursor(item->cursor(), old))
      item->setCursor(c);
}

//! Set while the theme replaces a widget's or a window's cursor: a widget
//! that answers with its standard shape again keeps it rather than looping.
const QObject* g_replacing{};

template <typename T>
void replaceCursor(T& target, const QCursor& c)
{
  if(g_replacing == &target)
    return;
  const QObject* previous = g_replacing;
  g_replacing = &target;
  target.setCursor(c);
  g_replacing = previous;
}
}

struct CursorTheme::Set
{
  //! 0 for none: what a stale cursor's entry points to.
  uint32_t id{};
  double screenScale{1.};
  std::array<QCursor, std::size_t(CursorArtwork::Count)> cursors;
  std::array<QPixmap, drag_artworks.size()> drag;
};

struct CursorTheme::Impl
{
  struct Known
  {
    CursorArtwork artwork{};
    //! Index in Set::drag for a drag cursor, -1 for a cursor.
    int8_t drag{-1};
    //! The set it belongs to, 0 when it was built for a previous zoom or
    //! for screens as they were.
    uint32_t set{};
  };

  //! The first is the set of the primary screen.
  std::vector<std::unique_ptr<Set>> sets;
  //! Every cursor and drag pixmap the theme built, by cache key.
  ossia::hash_map<qint64, Known> known;
  uint32_t nextId{1};
  double x11Scale{1.};

  //! The screen the pointer was last seen on.
  QRect pointerGeometry;
  double pointerScale{1.};
  bool pointerKnown{};

  //! The override cursor last looked at, for the screen under the pointer.
  qint64 overrideKey{};

  Set empty;
};

const CursorArtworkInfo& cursorArtworkInfo(CursorArtwork a) noexcept
{
  return artworks()[std::size_t(a)];
}

CursorArtwork cursorArtworkForShape(Qt::CursorShape s) noexcept
{
  if(s < Qt::ArrowCursor || s > Qt::LastCursor)
    return CursorArtwork::Count;
  return shape_artworks[s];
}

CursorPlatform cursorPlatform(const QString& platformName) noexcept
{
  if(platformName == QLatin1String("xcb"))
    return CursorPlatform::X11;
  if(platformName == QLatin1String("cocoa"))
    return CursorPlatform::MacOS;
  if(platformName.startsWith(QLatin1String("wayland")))
    return CursorPlatform::Wayland;
  return CursorPlatform::Windows;
}

CursorGeometry cursorGeometry(
    CursorPlatform platform, QSize canvas, QPointF hotspot, double zoom,
    double screenScale) noexcept
{
  if(!(zoom > 0.))
    zoom = 1.;
  if(!(screenScale > 0.))
    screenScale = 1.;

  // Wayland takes the buffer scale as an integer, and the compositor scales
  // the surface by the output's: the pixmap is drawn at the next integer
  // scale of the output, which the compositor shows pixel for pixel on
  // integer scales and scales down on fractional ones.
  if(platform == CursorPlatform::Wayland)
    screenScale = std::ceil(screenScale - 1e-6);

  const double pixelScale = zoom * screenScale;
  const auto px = [](double v) { return std::max(1, int(std::lround(v))); };

  CursorGeometry g;
  g.pixelSize = QSize{px(canvas.width() * pixelScale), px(canvas.height() * pixelScale)};

  // The pixel the hotspot point of the artwork falls in.
  const QPoint target{
      std::clamp(int(std::floor(hotspot.x() * pixelScale + 1e-6)), 0, g.pixelSize.width() - 1),
      std::clamp(int(std::floor(hotspot.y() * pixelScale + 1e-6)), 0, g.pixelSize.height() - 1)};

  switch(platform)
  {
    case CursorPlatform::X11:
      // qxcbcursor.cpp: the image and the hotspot go to XRender as they are.
      g.devicePixelRatio = 1.;
      g.hotspot = target;
      g.pixelHotspot = target;
      break;
    case CursorPlatform::Windows: {
      // qwindowscursor.cpp createPixmapCursor: the pixmap is scaled by
      // factor / devicePixelRatio, which is 1 here, and the hotspot is
      // qRound(hotspot * factor), the factor being zoom * screenScale.
      const auto qround = [](double v) { return int(std::floor(v + 0.5)); };
      g.devicePixelRatio = pixelScale;
      g.hotspot = QPoint{
          closestUnit(target.x(), pixelScale, qround),
          closestUnit(target.y(), pixelScale, qround)};
      g.pixelHotspot
          = QPoint{qround(g.hotspot.x() * pixelScale), qround(g.hotspot.y() * pixelScale)};
      break;
    }
    case CursorPlatform::MacOS:
    case CursorPlatform::Wayland: {
      // qcocoacursor.mm + NSImage imageFromQImage: the cursor measures
      // pixels / devicePixelRatio points, the hotspot is in points.
      // qwaylandinputdevice.cpp setCursor: buffer scale int(devicePixelRatio),
      // an integer here, so the hotspot is taken in surface coordinates.
      // A point is screenScale pixels in both.
      const auto lround = [](double v) { return int(std::lround(v)); };
      g.devicePixelRatio = screenScale;
      g.hotspot = QPoint{
          closestUnit(target.x(), screenScale, lround),
          closestUnit(target.y(), screenScale, lround)};
      g.pixelHotspot
          = QPoint{lround(g.hotspot.x() * screenScale), lround(g.hotspot.y() * screenScale)};
      break;
    }
  }
  g.renderOffset = g.pixelHotspot - target;
  return g;
}

QImage renderCursorArtwork(CursorArtwork a, const CursorGeometry& g)
{
  registerCursorResources();
  const auto& info = cursorArtworkInfo(a);
  QImage img{g.pixelSize, QImage::Format_ARGB32_Premultiplied};
  img.fill(Qt::transparent);

  QPainter p{&img};
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  p.translate(g.renderOffset);
  if(info.mirrored)
  {
    p.translate(g.pixelSize.width(), 0);
    p.scale(-1, 1);
  }
  const QRectF target{QPointF{}, QSizeF{g.pixelSize}};

#if SCORE_CURSORS_HAVE_SVG
  QSvgRenderer renderer{QString::fromLatin1(info.svg)};
  if(renderer.isValid())
  {
    renderer.render(&p, target);
    return img;
  }
#endif

  // Without QtSvg: the bitmaps rendered from the same sources, where they
  // exist, from the one with the most detail.
  const QString base = QFileInfo{QString::fromLatin1(info.svg)}.completeBaseName();
  for(const QString& candidate :
      {QStringLiteral(":/icons/%1@2x.png").arg(base),
       QStringLiteral(":/icons/%1.png").arg(base)})
  {
    if(QFile::exists(candidate))
    {
      p.drawImage(target, QImage{candidate});
      return img;
    }
  }
  return {};
}

QPixmap renderDragCursor(CursorArtwork a, const CursorGeometry& g, bool topLeft)
{
  QImage art = renderCursorArtwork(a, g);
  if(art.isNull())
    return {};

  if(topLeft)
  {
    // From the top left of what is drawn: the apex of an arrow with its
    // outline, the whole of the other artworks.
    QRect drawn;
    for(int y = 0; y < art.height(); y++)
    {
      const auto* line = reinterpret_cast<const QRgb*>(art.constScanLine(y));
      for(int x = 0; x < art.width(); x++)
        if(qAlpha(line[x]) != 0)
          drawn |= QRect{x, y, 1, 1};
    }
    if(drawn.isEmpty())
      drawn = QRect{QPoint{}, art.size()};
    return toPixmap(art.copy(drawn), 1.);
  }

  // The default hotspot of QCursor is width / 2 / devicePixelRatio in device
  // independent units: the hotspot pixel goes in the middle, at a multiple of
  // an integer ratio so that the division is exact.
  const QPoint hot = g.pixelHotspot;
  const int unit = std::abs(g.devicePixelRatio - std::round(g.devicePixelRatio)) < 1e-6
                       ? std::max(1, int(std::lround(g.devicePixelRatio)))
                       : 1;
  const auto upTo = [unit](int v) { return (v + unit - 1) / unit * unit; };
  const int hx = upTo(std::max(hot.x(), art.width() - hot.x()));
  const int hy = upTo(std::max(hot.y(), art.height() - hot.y()));
  QImage out{QSize{2 * hx, 2 * hy}, QImage::Format_ARGB32_Premultiplied};
  out.fill(Qt::transparent);
  QPainter p{&out};
  p.drawImage(QPoint{hx - hot.x(), hy - hot.y()}, art);
  p.end();
  return toPixmap(std::move(out), g.devicePixelRatio);
}

CursorArtwork windowsHitTestCursor(int hitTest, bool windowBlocked) noexcept
{
  namespace ht = win32_hit;
  switch(hitTest)
  {
    case ht::Client:
      // Windows disables a window blocked by a modal one, and no Qt event
      // gives it a cursor.
      return windowBlocked ? CursorArtwork::Pointer : CursorArtwork::Count;
    case ht::Error:
    case ht::Caption:
    case ht::SysMenu:
    case ht::Menu:
    case ht::HScroll:
    case ht::VScroll:
    case ht::MinButton:
    case ht::MaxButton:
    case ht::Border:
    case ht::Close:
    case ht::Help:
      return CursorArtwork::Pointer;
    case ht::Left:
    case ht::Right:
      return CursorArtwork::ScaleH;
    case ht::Top:
    case ht::Bottom:
      return CursorArtwork::ScaleV;
    case ht::TopLeft:
    case ht::BottomRight:
    case ht::GrowBox:
      return CursorArtwork::ScaleFDiag;
    case ht::TopRight:
    case ht::BottomLeft:
      return CursorArtwork::ScaleBDiag;
    case ht::Transparent:
    case ht::Nowhere:
    case ht::Object:
    default:
      return CursorArtwork::Count;
  }
}

CursorTheme& CursorTheme::instance()
{
  // Owned by the application: its cursors and its filter go with it, and a
  // process that creates another application, as the tests do, gets a theme
  // for that one.
  static QPointer<CursorTheme> theme;
  if(!theme)
  {
    if(!qGuiApp)
    {
      static CursorTheme without_application;
      return without_application;
    }
    theme = new CursorTheme;
    theme->setParent(qGuiApp);
    for(QScreen* screen : QGuiApplication::screens())
      theme->watchScreen(screen);
    connect(qGuiApp, &QGuiApplication::screenAdded, theme, [](QScreen* screen) {
      if(theme)
      {
        theme->watchScreen(screen);
        theme->screensChanged();
      }
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, theme, [] {
      if(theme)
        theme->screensChanged();
    });
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, theme, [] {
      if(theme)
        theme->screensChanged();
    });
  }
  return *theme;
}

void CursorTheme::watchScreen(QScreen* screen)
{
  if(!screen)
    return;
  // A screen whose scale changes (Windows' display settings, a wayland
  // output's scale, a monitor moved to another GPU) announces it through its
  // DPI; its windows get QEvent::DevicePixelRatioChange.
  connect(screen, &QScreen::logicalDotsPerInchChanged, this, [this] { screensChanged(); });
  connect(screen, &QScreen::physicalDotsPerInchChanged, this, [this] { screensChanged(); });
  connect(screen, &QScreen::geometryChanged, this, [this] { m_impl->pointerKnown = false; });
}

void CursorTheme::screensChanged()
{
  if(!m_built || !qGuiApp)
    return;
  m_impl->pointerKnown = false;
  if(m_platform == CursorPlatform::X11)
    return;
  const double primary = screenScaleOf(QGuiApplication::primaryScreen());
  if(std::abs(primary - defaultSet().screenScale) > same_scale)
    rebuild();
  else
    refixEverything();
}

void CursorTheme::setStartupZoom(double zoom) noexcept
{
  if(zoom > 0.)
    g_zoom = zoom;
}

CursorTheme::CursorTheme()
    : m_impl{std::make_unique<Impl>()}
    , m_zoom{g_zoom}
{
}

CursorTheme::~CursorTheme()
{
  if(m_installed)
    cursor_native::uninstall();
  cursor_native::release();
}

void CursorTheme::ensureBuilt() const
{
  if(!m_built)
    const_cast<CursorTheme*>(this)->build();
}

void CursorTheme::build()
{
  if(!qGuiApp)
    return;

  cursor_native::release();
  auto& impl = *m_impl;
  m_platform = cursorPlatform(QGuiApplication::platformName());
  impl.x11Scale = 1.;
  if(m_platform == CursorPlatform::X11)
    if(const double s = x11DesktopCursorScale(); s > 0.)
      impl.x11Scale = s;

  for(auto& [key, known] : impl.known)
    known.set = 0;
  impl.sets.clear();
  impl.pointerKnown = false;
  impl.overrideKey = 0;
  m_built = true;

  (void)set(screenScaleOf(QGuiApplication::primaryScreen()));
}

const CursorTheme::Set& CursorTheme::set(double screenScale) const
{
  ensureBuilt();
  auto& impl = *m_impl;
  if(!m_built)
    return impl.empty;
  if(m_platform == CursorPlatform::X11 && !impl.sets.empty())
    return *impl.sets.front();
  for(const auto& s : impl.sets)
    if(std::abs(s->screenScale - screenScale) < same_scale)
      return *s;

  auto made = std::make_unique<Set>();
  Set& s = *made;
  s.id = impl.nextId++;
  s.screenScale = screenScale;
  for(std::size_t i = 0; i < s.cursors.size(); i++)
  {
    const auto a = CursorArtwork(i);
    const auto& info = artworks()[i];
    const auto g = cursorGeometry(m_platform, info.size, info.hotspot, m_zoom, screenScale);
    QImage img = renderCursorArtwork(a, g);
    if(img.isNull())
      continue;
    s.cursors[i]
        = QCursor{toPixmap(std::move(img), g.devicePixelRatio), g.hotspot.x(), g.hotspot.y()};
    impl.known[s.cursors[i].pixmap().cacheKey()] = {a, -1, s.id};
  }

  const bool windowsDrag = QGuiApplication::platformName() == QLatin1String("windows");
  for(std::size_t i = 0; i < drag_artworks.size(); i++)
  {
    const auto a = drag_artworks[i].second;
    const auto& info = artworks()[std::size_t(a)];
    const auto g = cursorGeometry(m_platform, info.size, info.hotspot, m_zoom, screenScale);
    s.drag[i] = renderDragCursor(a, g, windowsDrag);
    if(!s.drag[i].isNull())
      impl.known[s.drag[i].cacheKey()] = {a, int8_t(i), s.id};
  }

  impl.sets.push_back(std::move(made));
  return s;
}

const CursorTheme::Set& CursorTheme::defaultSet() const
{
  ensureBuilt();
  if(m_impl->sets.empty())
    return m_impl->empty;
  return *m_impl->sets.front();
}

double CursorTheme::screenScaleOf(const QScreen* screen) const noexcept
{
  ensureBuilt();
  if(m_platform == CursorPlatform::X11)
    return m_impl->x11Scale;
  if(!screen)
    screen = QGuiApplication::primaryScreen();
  if(screen)
    if(const double dpr = screen->devicePixelRatio(); dpr > 0.)
      return dpr / m_zoom;
  return 1.;
}

double CursorTheme::screenScaleOf(const QWindow& window) const noexcept
{
  ensureBuilt();
  if(m_platform == CursorPlatform::X11)
    return m_impl->x11Scale;
  // The ratio of the window is the one Qt scales its cursor by: the factor
  // of its screen on Windows, the backing scale times the zoom on macOS.
  if(const double dpr = window.devicePixelRatio(); dpr > 0.)
    return dpr / m_zoom;
  return screenScaleOf(window.screen());
}

double CursorTheme::screenScaleOf(const QWidget& widget) const noexcept
{
  ensureBuilt();
  if(m_platform == CursorPlatform::X11)
    return m_impl->x11Scale;
  // A widget's cursor goes to the window of its closest native ancestor.
  const QWidget* native = widget.isWindow() ? &widget : widget.nativeParentWidget();
  if(native)
    if(const QWindow* w = native->windowHandle())
      return screenScaleOf(*w);
  return screenScaleOf(widget.screen());
}

double CursorTheme::screenScale() const noexcept
{
  return defaultSet().screenScale;
}

void CursorTheme::rebuild()
{
  if(!qGuiApp)
    return;
  if(!m_built)
  {
    build();
    return;
  }

  build();
  refixEverything();
}

void CursorTheme::setZoom(double zoom)
{
  if(!(zoom > 0.))
    return;
  m_zoom = zoom;
  g_zoom = zoom;
  rebuild();
}

const QCursor& CursorTheme::cursor(CursorArtwork a) const noexcept
{
  return defaultSet().cursors[std::size_t(a)];
}

const QCursor& CursorTheme::cursor(CursorArtwork a, double screenScale) const noexcept
{
  return set(screenScale).cursors[std::size_t(a)];
}

QCursor CursorTheme::cursor(Qt::CursorShape s) const noexcept
{
  const auto a = cursorArtworkForShape(s);
  if(a == CursorArtwork::Count)
    return QCursor{s};
  const QCursor& c = cursor(a);
  if(c.shape() != Qt::BitmapCursor)
    return QCursor{s};
  return c;
}

QPixmap CursorTheme::dragCursor(Qt::DropAction action) const noexcept
{
  return defaultSet().drag[dragIndex(action)];
}

QPixmap CursorTheme::dragCursor(Qt::DropAction action, double screenScale) const noexcept
{
  return set(screenScale).drag[dragIndex(action)];
}

void CursorTheme::updatePointerScreen(QPointF globalPos) const
{
  if(m_platform == CursorPlatform::X11)
    return;
  auto& impl = *m_impl;
  const QPoint pos = globalPos.toPoint();
  if(impl.pointerKnown && impl.pointerGeometry.contains(pos))
    return;
  QScreen* screen = QGuiApplication::screenAt(pos);
  if(!screen)
    return;
  impl.pointerGeometry = screen->geometry();
  impl.pointerScale = screenScaleOf(screen);
  impl.pointerKnown = true;
  impl.overrideKey = 0;
}

void CursorTheme::setDragCursors(QDrag& drag) const
{
  ensureBuilt();
  auto& impl = *m_impl;

  // QWindowsDrag builds its cursors for the screen under the pointer when
  // the drag starts and when a drag cursor changes; QBasicDrag shows them as
  // the override cursor, which the platform shows at the pointer's screen.
  const Set* s = nullptr;
  if(m_platform != CursorPlatform::X11)
  {
    updatePointerScreen(QCursor::pos());
    if(impl.pointerKnown)
      s = &set(impl.pointerScale);
    else if(auto* source = qobject_cast<QWidget*>(drag.source()))
      s = &set(screenScaleOf(*source));
  }
  if(!s)
    s = &defaultSet();

  const auto give = [&](Qt::DropAction action, const QPixmap& pm) {
    if(pm.isNull())
      return;
    const QPixmap current = drag.dragCursor(action);
    if(!current.isNull())
    {
      // A cursor of the drag's own stays; one of score's for another screen
      // is replaced.
      if(current.cacheKey() == pm.cacheKey())
        return;
      auto it = impl.known.find(current.cacheKey());
      if(it == impl.known.end() || it->second.drag < 0)
        return;
    }
    drag.setDragCursor(pm, action);
  };
  for(std::size_t i = 0; i < drag_artworks.size(); i++)
    give(drag_artworks[i].first, s->drag[i]);
  give(Qt::TargetMoveAction, s->drag[1]);
}

const QCursor* CursorTheme::remap(const QCursor& c, const Set& target) const
{
  if(c.shape() != Qt::BitmapCursor)
    return nullptr;
  auto& known = m_impl->known;
  auto it = known.find(c.pixmap().cacheKey());
  if(it == known.end() || it->second.drag >= 0 || it->second.set == target.id)
    return nullptr;
  const QCursor& n = target.cursors[std::size_t(it->second.artwork)];
  return n.shape() == Qt::BitmapCursor ? &n : nullptr;
}

void CursorTheme::refixEverything()
{
  m_impl->overrideKey = 0;
  if(qobject_cast<QApplication*>(qGuiApp))
  {
    std::vector<QGraphicsView*> views;
    for(QWidget* w : QApplication::allWidgets())
    {
      if(w->isWindow() || w->testAttribute(Qt::WA_SetCursor))
        fixWidget(*w);
      if(auto* view = qobject_cast<QGraphicsView*>(w); view && view->scene())
        views.push_back(view);
    }

    for(QGraphicsView* view : views)
    {
      const Set& s = set(screenScaleOf(*view->viewport()));
      for(QGraphicsItem* item : view->scene()->items())
        if(item->hasCursor())
          if(const QCursor* n = remap(item->cursor(), s))
            item->setCursor(*n);
    }
  }

  for(QWindow* w : QGuiApplication::allWindows())
    fixWindow(*w);

  fixOverrideCursor();
}

void CursorTheme::fixWidget(QWidget& w) const
{
  if(!w.testAttribute(Qt::WA_SetCursor))
  {
    // A window without a cursor shows the platform's arrow, and so do its
    // children without one.
    if(w.isWindow())
      if(const QCursor& c = set(screenScaleOf(w)).cursors[std::size_t(CursorArtwork::Pointer)];
         c.shape() == Qt::BitmapCursor)
        replaceCursor(w, c);
    return;
  }

  const QCursor current = w.cursor();
  const auto shape = current.shape();
  if(shape == Qt::BitmapCursor)
  {
    // One of score's cursors built for another screen, or for a previous
    // zoom and kept somewhere the rebuild does not reach, such as the
    // cursor a QGraphicsView restores.
    if(const QCursor* n = remap(current, set(screenScaleOf(w))))
    {
      replaceItemCursors(w, m_lastMoveTarget, m_lastMoveLocal, m_lastMousePos, current, *n);
      replaceCursor(w, *n);
    }
    return;
  }

  if(!isThemeCursorShape(shape) || keepsStandardShape(w, shape))
    return;
  const QCursor& c
      = set(screenScaleOf(w)).cursors[std::size_t(cursorArtworkForShape(shape))];
  if(c.shape() != Qt::BitmapCursor)
    return;
  replaceItemCursors(w, m_lastMoveTarget, m_lastMoveLocal, m_lastMousePos, current, c);
  replaceCursor(w, c);
}

void CursorTheme::fixWindow(QWindow& w) const
{
  const QCursor current = w.cursor();
  const auto shape = current.shape();
  if(shape == Qt::BitmapCursor)
  {
    if(const QCursor* n = remap(current, set(screenScaleOf(w))))
      replaceCursor(w, *n);
    return;
  }

  // An unset cursor reads as Qt::ArrowCursor: the window then shows the
  // platform's arrow.
  if(!isThemeCursorShape(shape))
    return;
  const QCursor& c = set(screenScaleOf(w)).cursors[std::size_t(cursorArtworkForShape(shape))];
  if(c.shape() == Qt::BitmapCursor)
    replaceCursor(w, c);
}

void CursorTheme::fixOverrideCursor() const
{
  // Qt sets override cursors without an event: Wait in QTextBrowser,
  // WhatsThis and Forbidden in QWhatsThis, the cursor of
  // QWidget::grabMouse(QCursor), the cursors of drag and drop. They are
  // replaced at the next event, with those of the screen under the pointer,
  // where the platforms show them.
  const QCursor* c = QGuiApplication::overrideCursor();
  if(!c)
    return;
  const auto shape = c->shape();
  auto& impl = *m_impl;
  if(shape == Qt::BitmapCursor)
  {
    if(m_platform == CursorPlatform::X11)
      return;
    const qint64 key = c->pixmap().cacheKey();
    if(key == impl.overrideKey)
      return;
    impl.overrideKey = key;
  }
  else if(!isThemeCursorShape(shape))
  {
    return;
  }

  // What follows sends events of its own: the first QDragManager::self()
  // creates the manager as a child of the application.
  static bool fixing = false;
  if(fixing)
    return;
  fixing = true;
  struct reset
  {
    ~reset() { fixing = false; }
  } r;

  if(!impl.pointerKnown)
    updatePointerScreen(QCursor::pos());
  const Set& s = impl.pointerKnown ? set(impl.pointerScale) : defaultSet();

  int idx = -1;
  CursorArtwork artwork = CursorArtwork::Count;
  if(shape == Qt::BitmapCursor)
  {
    auto it = impl.known.find(impl.overrideKey);
    if(it == impl.known.end() || it->second.set == s.id)
      return;
    idx = it->second.drag;
    artwork = it->second.artwork;
  }
  else
  {
    idx = dragIndex(shape);
    artwork = cursorArtworkForShape(shape);
  }

#if SCORE_CURSORS_HAVE_DRAG_MANAGER
  if(idx >= 0)
  {
    if(auto* manager = QDragManager::self())
    {
      if(QDrag* drag = manager->object())
      {
        // QBasicDrag sets the standard shape, or the drag's cursor for the
        // action, again at the next move: the drag gets score's for this
        // screen, and shows the one that matches.
        setDragCursors(*drag);
        const QPixmap pm = drag->dragCursor(drag_artworks[idx].first);
        if(!pm.isNull())
        {
          QGuiApplication::changeOverrideCursor(QCursor{pm});
          impl.overrideKey = pm.cacheKey();
          return;
        }
      }
    }
  }
#endif
  if(shape == Qt::BitmapCursor && idx >= 0)
    return;

  const QCursor& mapped = s.cursors[std::size_t(artwork)];
  if(mapped.shape() == Qt::BitmapCursor)
  {
    QGuiApplication::changeOverrideCursor(mapped);
    impl.overrideKey = mapped.pixmap().cacheKey();
  }
}

bool CursorTheme::eventFilter(QObject* watched, QEvent* event)
{
  switch(event->type())
  {
    case QEvent::MouseMove: {
      // QGraphicsView sets its viewport's cursor while it handles the move.
      const auto& move = *static_cast<QMouseEvent*>(event);
      m_lastMousePos = move.globalPosition();
      m_lastMoveTarget = watched;
      m_lastMoveLocal = move.position();
      updatePointerScreen(m_lastMousePos);
#if defined(__APPLE__)
      if(watched->isWindowType())
        if(auto& w = *static_cast<QWindow*>(watched); cursor_native::showsCursorItself(w))
          cursor_native::showCursorOf(w);
#endif
      break;
    }

    case QEvent::CursorChange:
      if(watched->isWidgetType())
      {
        auto& w = *static_cast<QWidget*>(watched);
        if(qobject_cast<QMainWindow*>(&w))
        {
          // While a separator is hovered, QMainWindow gives itself its
          // separator cursor again whenever its cursor, or its window's,
          // changes to another shape, which score's would do forever. Its
          // cursor changes are kept from it: it then only misses those made
          // while a separator is hovered, which it would restore after.
          if(g_replacing != &w)
            fixWidget(w);
          return true;
        }
        fixWidget(w);
        if(!w.isEnabled() && !g_replacing)
          followEnabledState(w);
      }
      else if(watched->isWindowType())
      {
        auto& w = *static_cast<QWindow*>(watched);
        fixWindow(w);
#if defined(__APPLE__)
        if(cursor_native::showsCursorItself(w))
          cursor_native::showCursorOf(w);
#endif
      }
      break;

    case QEvent::EnabledChange:
      if(watched->isWidgetType())
        followEnabledState(*static_cast<QWidget*>(watched));
      break;

    case QEvent::Show:
    case QEvent::Enter:
    case QEvent::ScreenChangeInternal:
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    case QEvent::DevicePixelRatioChange:
#endif
      // A window shown, entered, or moved to a screen of another scale gets
      // the cursors of its screen. Qt sends the screen changes to every
      // widget of the window.
      if(watched->isWidgetType())
      {
        auto& w = *static_cast<QWidget*>(watched);
        if(w.isWindow()
           || (event->type() != QEvent::Show && event->type() != QEvent::Enter
               && w.testAttribute(Qt::WA_SetCursor)))
          fixWidget(w);
      }
      else if(watched->isWindowType())
      {
        auto& w = *static_cast<QWindow*>(watched);
        fixWindow(w);
#if defined(__APPLE__)
        if(event->type() == QEvent::Enter && cursor_native::showsCursorItself(w))
          cursor_native::showCursorOf(w);
#endif
      }
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
      if(event->type() == QEvent::DevicePixelRatioChange)
        m_impl->pointerKnown = false;
#endif
      break;

#if SCORE_CURSORS_HAVE_DRAG_MANAGER
    case QEvent::DragEnter:
      // Delivered before QBasicDrag picks the cursor for the action the
      // target accepts; the target may be on another screen than the source.
      if(auto* manager = QDragManager::self())
        if(QDrag* drag = manager->object())
          setDragCursors(*drag);
      break;
#endif

    default:
      break;
  }

  fixOverrideCursor();
  return false;
}

void CursorTheme::install()
{
  if(m_installed || !qGuiApp)
    return;
  ensureBuilt();
  m_installed = true;
  qGuiApp->installEventFilter(this);
  cursor_native::install(*this);

  if(qobject_cast<QApplication*>(qGuiApp))
    for(QWidget* w : QApplication::allWidgets())
      if(w->isWindow() || w->testAttribute(Qt::WA_SetCursor))
        fixWidget(*w);
  for(QWindow* w : QGuiApplication::allWindows())
    fixWindow(*w);
  fixOverrideCursor();
}
}
