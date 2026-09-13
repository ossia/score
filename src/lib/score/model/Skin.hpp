#pragma once
#include <QBrush>
#include <QCursor>
#include <QFont>
#include <QObject>
#include <QPair>
#include <QPen>
#include <QSize>
#include <QVector>

#include <score_lib_base_export.h>

#include <verdigris>

#include <functional>
#include <utility>
#include <vector>
class QJsonObject;
namespace score
{
struct Brush;
class Skin;
struct SCORE_LIB_BASE_EXPORT BrushSet
{
  QBrush brush;
  QPen pen_cosmetic;
  QPen pen0;
  QPen pen0_solid_round;
  QPen pen1;
  QPen pen1_dotted;
  QPen pen1_solid_flat_miter;
  QPen pen1_5;
  QPen pen2;
  QPen pen2_solid_round_round;
  QPen pen2_solid_flat_miter;
  QPen pen2_dashdot_square_miter;
  QPen pen2_dotted_square_miter;
  QPen pen3;
  QPen pen3_solid_flat_miter;
  QPen pen3_solid_round_round;
  QPen pen3_dashed_flat_miter;

  void setupPens();

private:
  BrushSet() noexcept;
  BrushSet(const BrushSet&) noexcept;
  BrushSet(BrushSet&&) noexcept;
  BrushSet& operator=(const BrushSet&) noexcept;
  BrushSet& operator=(BrushSet&&) noexcept;
  ~BrushSet();

  explicit BrushSet(const QBrush& b) noexcept;

  BrushSet& operator=(const QBrush& b) noexcept;
  friend struct Brush;
  friend class Skin;
};

struct SCORE_LIB_BASE_EXPORT Brush
{
  operator const QBrush&() const noexcept { return main.brush; }
  QColor color() const noexcept { return main.brush.color(); }

  BrushSet main;
  BrushSet darker;
  BrushSet darker300;
  BrushSet lighter;
  BrushSet lighter180;

  void reload(QColor color) noexcept;

  struct Pair;

private:
  Brush() noexcept;
  Brush(const Brush&) noexcept;
  Brush(Brush&&) noexcept;
  Brush& operator=(const Brush&) noexcept;
  Brush& operator=(Brush&&) noexcept;
  ~Brush();

  explicit Brush(const QBrush& b) noexcept;
  Brush& operator=(const QBrush& b) noexcept;

  friend class Skin;
};
struct Brush::Pair
{
  Pair(QString&& str, QColor&& c)
      : first{std::move(str)}
      , second{std::move(c)}
  {
  }
  QString first;
  Brush second;
};
class SCORE_LIB_BASE_EXPORT Skin : public QObject
{
  W_OBJECT(Skin)
public:
  static Skin& instance() noexcept;
  ~Skin() override;

  //! Which halves of a skin file to apply. Colours and fonts are separable
  //! so that trying a different palette does not also change every font, and
  //! vice versa.
  enum Part
  {
    Colours = 1,
    Fonts = 2,
    Everything = Colours | Fonts
  };

  void load(const QJsonObject& style, int parts = Everything);

  //! Colours and fonts, in the form load() reads back. Used to write skin
  //! files, so anything added here must have a load() counterpart.
  QJsonObject toJson() const;

  //! Every font a skin may name, paired with the key it uses in a skin file.
  //! One list, so load, save, the defaults and the skin editor cannot drift
  //! apart. Writing through these pointers then emitting changed() is how the
  //! editor applies a font live.
  std::vector<std::pair<const char*, QFont*>> fonts() noexcept;

  //! Font for the widget UI as a whole. A skin may override it; applying it
  //! to the QApplication is score::setupApplicationFont()'s job, since the
  //! per-widget-class font hash has to be reseeded too.
  QFont ApplicationFont;

  QFont SansFont;
  QFont MonoFont;
  QFont MonoFontSmall;
  QFont SansFontSmall;

  QFont Bold10Pt;
  QFont Bold12Pt;
  QFont Medium7Pt;
  QFont Medium8Pt;
  QFont Medium10Pt;
  QFont Medium12Pt;

  QFont TitleFont;

  //! The heading over an inspector page -- "Interval (foo)", "Process (bar)".
  //! Its own role because it is bold text one step above the body, and a
  //! pixel font cannot be emboldened or resized freely: both would have to be
  //! faked, which smears the one-pixel stems these fonts are drawn with.
  QFont SectionTitleFont;

  //! The transport bar's time readout. Its own role because it is the one
  //! large piece of text in the UI: a pixel-font skin wants it on the grid
  //! and unantialiased, which a scaled-up body font cannot give.
  QFont TimecodeFont;

  //! Script and shader editors. Monospaced and usually a size of its own,
  //! since code wants more lines on screen than a settings form does.
  QFont CodeFont;

  Brush Dark;
  Brush HalfDark;
  Brush DarkGray;
  Brush Gray;
  Brush LightGray;
  Brush HalfLight;
  Brush Light;

  Brush Emphasis1;
  Brush Emphasis2;
  Brush Emphasis3;
  Brush Emphasis4;
  Brush Emphasis5;

  Brush Base1;
  Brush Base2;
  Brush Base3;
  Brush Base4;
  Brush Base5;

  Brush Warn1;
  Brush Warn2;
  Brush Warn3;

  Brush Background1;
  Brush Background2;

  Brush Transparent1;
  Brush Transparent2;
  Brush Transparent3;

  Brush Smooth1;
  Brush Smooth2;
  Brush Smooth3;

  Brush Tender1;
  Brush Tender2;
  Brush Tender3;

  Brush Cable1;
  Brush Cable2;
  Brush Cable3;

  Brush SelectedCable1;
  Brush SelectedCable2;
  Brush SelectedCable3;

  Brush Port1;
  Brush Port2;
  Brush Port3;

  Brush Pulse1;
  Brush Pulse2;

  const QPen TransparentPen;
  const QBrush TransparentBrush;
  const QPen NoPen;
  const QBrush NoBrush;
  QBrush TextBrush;

  QPen TextItemPen;

  QBrush SliderBrush;
  QPen SliderPen;
  QBrush SliderInteriorBrush;
  QPen SliderLine;
  QPen SliderTextPen;
  QFont SliderFont;

  QCursor CursorPointer;
  QCursor CursorOpenHand;
  QCursor CursorClosedHand;
  QCursor CursorPointingHand;
  QCursor CursorMagnifier;
  QCursor CursorMove;
  QCursor CursorScaleH;
  QCursor CursorScaleV;
  QCursor CursorScaleFDiag;
  QCursor CursorSpin;

  QCursor CursorPlayFromHere;
  QCursor CursorCreationMode;

  const Brush* fromString(const QString& s) const;
  Brush* fromString(const QString& s);
  QString toString(const Brush*) const;

  QVector<QPair<QColor, QString>> getColors() const;
  QVector<QPair<QColor, QString>> getDefaultPaletteColors() const;

  // In [0; 25[
  int PulseIndex{};

  int LoadIndex{};

public:
  void changed() E_SIGNAL(SCORE_LIB_BASE_EXPORT, changed)

private:
  void timerEvent(QTimerEvent* event) override;
  Skin() noexcept;

  struct NoGUI
  {
  };
  explicit Skin(NoGUI);

  //! Builds every font member from the built-in defaults plus the
  //! Skin/Font* QSettings. Called by the constructor, and again by load()
  //! so that switching skins does not inherit the previous skin's fonts.
  void setupFonts();

  //! Applies the "fonts" object of a skin file over the defaults.
  void loadFonts(const QJsonObject& spec_obj);

  //! Serialises every font member, for toJson().
  QJsonObject saveFonts() const;

  struct color_map;
  color_map* initColorMap() noexcept;
  color_map* m_colorMap{};
  std::vector<Brush::Pair> m_defaultPalette;

  bool m_pulseDirection{false};
};

//! The application font size used before any skin has loaded. Not a setting:
//! font sizes are expressed in the skin, and the "application" role replaces
//! this as soon as one loads.
SCORE_LIB_BASE_EXPORT int uiFontSize() noexcept;

//! The application font size every hardcoded length in the widget UI was
//! drawn against. Icon edges, toolbar heights, the paddings hand-painted
//! inside the custom controls: all of them were picked with 13 px text on
//! screen, and none of them mean the same thing next to 8 px text.
inline constexpr int referenceFontSize = 13;

//! How much bigger or smaller the current skin's text is than that.
//!
//! A skin is free to ask for 8 px text, but the boxes around it have to
//! follow or the UI turns into padding with a few characters in it. Read
//! from the application font, which the "application" role drives: it is the
//! one font every skin sets, and the one the user thinks of as "the font
//! size".
SCORE_LIB_BASE_EXPORT double fontScale() noexcept;

//! A length measured against referenceFontSize, in the current skin's terms.
//! Clamped to at least one pixel: a control scaled out of existence is worse
//! than one a pixel too wide.
SCORE_LIB_BASE_EXPORT int scaledPixels(int px) noexcept;

//! Shorthand for the common call site, setIconSize(score::scaledIcon(24)).
SCORE_LIB_BASE_EXPORT QSize scaledIcon(int px) noexcept;

//! Runs \p f now, and again on every skin change, for the widget geometry
//! that is derived from the fonts.
//!
//! Sizes set once at construction are wrong by the time the user sees them:
//! the main window and the toolbars are built while the application font is
//! still the bootstrap one, and the skin -- with the font size the user
//! actually chose -- only loads once the scenario plugin's settings do.
//!
//! Safe to call while the window is being built. Skin::instance() needs the
//! application context, which does not exist that early, so the subscription
//! itself is made on the next turn of the event loop.
SCORE_LIB_BASE_EXPORT void onSkinChange(QObject* owner, std::function<void()> f);

//! An icon size that follows the skin's font. \p px is the edge the icon had
//! when every length in the UI was written against referenceFontSize.
template <typename T>
void setSkinIconSize(T* widget, int px)
{
  onSkinChange(widget, [widget, px] { widget->setIconSize(scaledIcon(px)); });
}

//! Hinting for the pre-skin font, from the Skin/FontHinting setting. A skin
//! can set hinting per role, which wins over this.
SCORE_LIB_BASE_EXPORT QFont::HintingPreference uiFontHinting() noexcept;

//! NoSubpixelAntialias on macOS: QCocoaScreen rewrites Subpixel_None to
//! Subpixel_RGB, so the style strategy is the only way to get grayscale AA.
SCORE_LIB_BASE_EXPORT QFont::StyleStrategy uiFontStyleStrategy() noexcept;

//! The design grid, in pixels, of a pixel font score ships, or 0 for anything
//! else. These fonts are drawn on a grid and only render sharply at whole
//! multiples of it; the skin editor uses this to say which sizes are usable.
SCORE_LIB_BASE_EXPORT int pixelFontGrid(const QString& family) noexcept;

//! \p px snapped to a size at which \p f's family renders sharply: rounded
//! down to a whole multiple of its grid, or up to a single step when \p px is
//! smaller than one. \p px unchanged for an outline font.
//!
//! Code that computes a font size instead of taking one from the skin has to
//! go through this. A pixel font at a size that is not a whole multiple of
//! its grid puts every outline between pixels, and the label comes out with
//! stems of two different widths -- which is the whole thing these fonts are
//! chosen to avoid.
SCORE_LIB_BASE_EXPORT int snapToFontGrid(const QFont& f, int px) noexcept;

//! setPixelSize through snapToFontGrid.
SCORE_LIB_BASE_EXPORT void setSnappedPixelSize(QFont& f, int px) noexcept;

//! Registers every font in the :/fonts resource with the QFontDatabase, so
//! that a skin naming one of them resolves instead of falling back. Idempotent,
//! and called by the Skin itself, so tests and alternate hosts get the fonts
//! without going through the application bootstrap.
SCORE_LIB_BASE_EXPORT void registerApplicationFonts();

//! The application font built from the Skin/Font* settings alone. Does not
//! touch Skin::instance(), so it is callable during early application startup,
//! before the application context exists.
SCORE_LIB_BASE_EXPORT QFont defaultApplicationFont() noexcept;

//! Whether setGlobalScaleFactor() can actually do anything in this build.
//!
//! False below Qt 6.6, or when Qt was built without high-DPI scaling. The
//! private QHighDpiScaling API has existed since Qt 5.6, but setGlobalFactor()
//! only started updating the screens in 6.5 and only started emitting the
//! QScreen change signals in 6.6; before that it sets a field nothing reads.
//! Use this to decide whether a zoom control can apply live or has to say
//! "needs restart".
SCORE_LIB_BASE_EXPORT bool canSetGlobalScaleFactorLive() noexcept;

//! Changes the global high-DPI scale factor of a running application, the
//! thing QT_SCALE_FACTOR sets at startup.
//!
//! Goes through QHighDpiScaling, which is Qt private API: it updates each
//! screen's geometry, which propagates a re-layout to every window. Qt warns
//! when this is called with windows open, since it is meant for startup, so
//! expect a message on the console; the factor is still applied.
//!
//! Everything cached at a given device pixel ratio has to be dropped
//! afterwards, which is why this bumps the Skin and emits changed(): the
//! glyph and pixmap caches hang off that signal. Returns false if the factor
//! is out of range or scaling is unavailable in this Qt build.
SCORE_LIB_BASE_EXPORT bool setGlobalScaleFactor(double factor);

//! Sets the application-wide font and reseeds the per-widget-class fonts the
//! platform theme installs, which would otherwise override it. Must run after
//! QApplication::setStyle(), which resets that hash. Safe to call again when
//! the skin changes.
SCORE_LIB_BASE_EXPORT void setupApplicationFont(const QFont& f);

}
