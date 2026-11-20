#pragma once
#include <QCursor>
#include <QObject>
#include <QPixmap>
#include <QPointF>
#include <QSize>

#include <score_lib_base_export.h>

#include <array>
#include <cstdint>
#include <memory>

class QDrag;
class QImage;
class QScreen;
class QWidget;
class QWindow;

namespace score
{
/**
 * Every cursor score shows is drawn from score's own artwork: the platforms'
 * cursor themes neither follow the graphical zoom nor match score's look.
 *
 * The artwork is vector, laid out on a 32 pixel canvas, and rendered at the
 * exact pixel size the platform will show on the screen it is shown on, so
 * that a cursor measures its canvas size times the zoom times the scale of
 * that screen, and stays sharp at fractional zooms and on every screen of a
 * desktop whose screens have different scales.
 */
enum class CursorArtwork : uint8_t
{
  Pointer,
  UpArrow,
  Cross,
  Wait,
  IBeam,
  ScaleV,
  ScaleH,
  ScaleBDiag,
  ScaleFDiag,
  Move,
  SplitV,
  SplitH,
  PointingHand,
  Forbidden,
  WhatsThis,
  Busy,
  OpenHand,
  ClosedHand,
  DragCopy,
  DragMove,
  DragLink,
  Magnifier,
  Spin,
  PlayFromHere,
  CreationMode,

  Count
};

struct CursorArtworkInfo
{
  //! Path of the SVG source in the resources.
  const char* svg{};
  //! Size of the canvas the artwork is drawn on, at a zoom of 1 on a screen
  //! of scale 1.
  QSize size{32, 32};
  //! The point of the artwork that clicks, in canvas units: the apex of an
  //! arrow, the centre of a cross. The hotspot is the pixel this point falls
  //! in once rendered.
  QPointF hotspot{};
  //! Drawn as the horizontal mirror of the SVG; the hotspot is given for the
  //! mirrored drawing.
  bool mirrored{};
};

SCORE_LIB_BASE_EXPORT
const CursorArtworkInfo& cursorArtworkInfo(CursorArtwork) noexcept;

//! The artwork that replaces a standard shape; Count for Qt::BlankCursor and
//! the shapes that are not standard (Qt::BitmapCursor, Qt::CustomCursor).
SCORE_LIB_BASE_EXPORT
CursorArtwork cursorArtworkForShape(Qt::CursorShape) noexcept;

//! The shapes of Qt::CursorShape that the platform draws from its own theme.
constexpr bool isThemeCursorShape(Qt::CursorShape s) noexcept
{
  return s >= Qt::ArrowCursor && s <= Qt::LastCursor && s != Qt::BlankCursor;
}

//! How a platform plug-in of Qt turns a pixmap cursor into the one on screen.
enum class CursorPlatform : uint8_t
{
  //! xcb: the pixmap is shown pixel for pixel, the hotspot is in pixels.
  X11,
  //! The pixmap is rescaled by the scale factor of the window's screen over
  //! its device pixel ratio, the hotspot is in device independent pixels and
  //! multiplied by that factor. Also the default for the other plug-ins,
  //! which go through the same device independent coordinates.
  Windows,
  //! cocoa: the cursor measures the pixmap's size over its device pixel
  //! ratio in points, the hotspot is in points.
  MacOS,
  //! wayland: the buffer scale is the integer part of the pixmap's device
  //! pixel ratio, the hotspot is in surface coordinates (multiplied by the
  //! fractional part of the ratio if there is one).
  Wayland
};

SCORE_LIB_BASE_EXPORT
CursorPlatform cursorPlatform(const QString& platformName) noexcept;

struct CursorGeometry
{
  //! Size and device pixel ratio of the pixmap to build the QCursor from.
  QSize pixelSize;
  qreal devicePixelRatio{1.};
  //! What to give QCursor as hotspot on that platform.
  QPoint hotspot;
  //! The pixel of the pixmap the platform puts the hotspot on.
  QPoint pixelHotspot;
  //! Whole pixels the artwork is moved by when rendered, so that its hotspot
  //! point falls in pixelHotspot: the platforms that take the hotspot in
  //! device independent units cannot name every pixel.
  QPoint renderOffset;
};

/**
 * Size, device pixel ratio and hotspot of a cursor drawn on a canvas of
 * \p canvas units whose point \p hotspot clicks, so that the platform shows
 * it at canvas * zoom * screenScale device pixels, pixel for pixel.
 *
 * \p zoom is the graphical zoom of score (the global scale factor of Qt).
 * \p screenScale is the scale of the screen without the zoom: the device
 * pixel ratio of the screen over the zoom, except on X11 where it is the
 * desktop's pointer size over 24, as X11 has no scale for the pointer.
 */
SCORE_LIB_BASE_EXPORT
CursorGeometry cursorGeometry(
    CursorPlatform platform, QSize canvas, QPointF hotspot, double zoom,
    double screenScale) noexcept;

//! The artwork rendered with a geometry from cursorGeometry(): the image the
//! QCursor is built from. Null without the artwork's source.
SCORE_LIB_BASE_EXPORT
QImage renderCursorArtwork(CursorArtwork a, const CursorGeometry& g);

/**
 * Pixmap for QDrag::setDragCursor. QBasicDrag (X11, Wayland and the other
 * plug-ins built on it) makes a QCursor of it with the default hotspot, the
 * centre, so the artwork is laid out around its hotspot. QWindowsDrag shows
 * it pixel for pixel with its top left corner at the pointer
 * (\p topLeft): the artwork starts at its own top left, which is the apex for
 * the arrows. Its device pixel ratio is then 1, and its size the one for the
 * screen the pointer is on.
 */
SCORE_LIB_BASE_EXPORT
QPixmap renderDragCursor(CursorArtwork a, const CursorGeometry& g, bool topLeft);

/**
 * Win32 hit-test codes (WM_NCHITTEST), as found sign-extended in the low word
 * of WM_SETCURSOR's lParam.
 */
namespace win32_hit
{
constexpr int Error = -2;
constexpr int Transparent = -1;
constexpr int Nowhere = 0;
constexpr int Client = 1;
constexpr int Caption = 2;
constexpr int SysMenu = 3;
constexpr int GrowBox = 4;
constexpr int Menu = 5;
constexpr int HScroll = 6;
constexpr int VScroll = 7;
constexpr int MinButton = 8;
constexpr int MaxButton = 9;
constexpr int Left = 10;
constexpr int Right = 11;
constexpr int Top = 12;
constexpr int TopLeft = 13;
constexpr int TopRight = 14;
constexpr int Bottom = 15;
constexpr int BottomLeft = 16;
constexpr int BottomRight = 17;
constexpr int Border = 18;
constexpr int Object = 19;
constexpr int Close = 20;
constexpr int Help = 21;
}

/**
 * The cursor score shows for a WM_SETCURSOR no Qt event reaches: the frame
 * and title bar of its windows, and the whole of a window blocked by a modal
 * one, which Windows disables. Count where Qt or the system decide: the
 * client area of a window that is not blocked, and what is not score's.
 */
SCORE_LIB_BASE_EXPORT
CursorArtwork windowsHitTestCursor(int hitTest, bool windowBlocked) noexcept;

/**
 * Builds the cursors and puts them everywhere a standard shape would reach
 * the platform: widgets, windows (QML included), the override cursor and
 * the cursors of drag and drop.
 *
 * A set of cursors is built for each scale of screen something is shown on,
 * when first needed: the platforms show a window's cursor at the scale of the
 * window's screen (one scale for the whole desktop on X11), and Qt resamples
 * a pixmap made for another one. Each widget and window gets the set of its
 * screen, again when it moves to another screen; the override cursor and the
 * cursors of a drag get the set of the screen under the pointer.
 */
class SCORE_LIB_BASE_EXPORT CursorTheme final : public QObject
{
public:
  static CursorTheme& instance();

  //! The zoom the cursors are built for: score's graphical zoom. Callable
  //! before the QGuiApplication exists.
  static void setStartupZoom(double zoom) noexcept;

  //! Rebuilds the cursors for a new zoom, and puts the new ones in place of
  //! the old ones on every widget, window and graphics item.
  void setZoom(double zoom);
  double zoom() const noexcept { return m_zoom; }

  //! Rebuilds the cursors for the screens as they are now.
  void rebuild();

  //! The cursors of the primary screen, which score's code sets: the theme
  //! swaps them for those of the screen they are shown on.
  const QCursor& cursor(CursorArtwork a) const noexcept;
  //! The cursor shown for a standard shape; the shape itself for
  //! Qt::BlankCursor and for the shapes that are not standard.
  QCursor cursor(Qt::CursorShape s) const noexcept;

  //! The cursor of a screen scale (see cursorGeometry()), built if needed.
  const QCursor& cursor(CursorArtwork a, double screenScale) const noexcept;

  //! The scale cursors are built at for what is shown on a screen, in a
  //! window or in a widget.
  double screenScaleOf(const QScreen* screen) const noexcept;
  double screenScaleOf(const QWindow& window) const noexcept;
  double screenScaleOf(const QWidget& widget) const noexcept;

  //! Pixmap QDrag::setDragCursor expects for a drop action on the platform,
  //! for the primary screen or for a screen scale.
  QPixmap dragCursor(Qt::DropAction action) const noexcept;
  QPixmap dragCursor(Qt::DropAction action, double screenScale) const noexcept;

  //! Gives a drag score's cursors, for the screen the pointer is on, for
  //! every action it does not have a cursor of its own for.
  void setDragCursors(QDrag& drag) const;

  /**
   * Installs the application wide event filter, and replaces the standard
   * cursors already set on existing widgets and windows. Needs a
   * QGuiApplication.
   */
  void install();
  bool installed() const noexcept { return m_installed; }

  //! Replaces a standard cursor shape, or a cursor of score made for another
  //! zoom or screen, on one widget or window, and gives the top-level ones
  //! that have none score's pointer. Used by the filter.
  void fixWidget(QWidget& w) const;
  void fixWindow(QWindow& w) const;

  //! Geometry the cursors of the primary screen are built with.
  CursorPlatform platform() const noexcept { return m_platform; }
  double screenScale() const noexcept;

private:
  struct Set;
  struct Impl;

  CursorTheme();
  ~CursorTheme() override;
  bool eventFilter(QObject* watched, QEvent* event) override;

  void build();
  void watchScreen(QScreen* screen);
  void screensChanged();
  void refixEverything();
  void fixOverrideCursor() const;
  void updatePointerScreen(QPointF globalPos) const;
  void ensureBuilt() const;

  const Set& set(double screenScale) const;
  const Set& defaultSet() const;
  const QCursor* remap(const QCursor& c, const Set& target) const;

  std::unique_ptr<Impl> m_impl;
  QPointF m_lastMousePos;
  //! Compared with, never dereferenced.
  const QObject* m_lastMoveTarget{};
  QPointF m_lastMoveLocal;
  double m_zoom{1.};
  CursorPlatform m_platform{CursorPlatform::Windows};
  bool m_installed{};
  bool m_built{};
};
}
