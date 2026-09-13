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

  //! Which halves of a skin file to apply.
  enum Part
  {
    Colours = 1,
    Fonts = 2,
    Everything = Colours | Fonts
  };

  void load(const QJsonObject& style, int parts = Everything);

  //! In the form load() reads back: anything added here needs a load()
  //! counterpart.
  QJsonObject toJson() const;

  //! Every font a skin may name, keyed as in the file. Single list so load,
  //! save, the defaults and the editor cannot drift apart; the editor writes
  //! through these pointers and emits changed().
  std::vector<std::pair<const char*, QFont*>> fonts() noexcept;

  //! Applying this to the QApplication is setupApplicationFont()'s job.
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

  //! Inspector page heading. Separate role: a pixel font can be neither
  //! emboldened nor resized freely without Qt faking it.
  QFont SectionTitleFont;

  //! Transport time readout. Separate role: the one large piece of text, so
  //! a pixel-font skin needs it on its own grid.
  QFont TimecodeFont;

  //! Timeline ruler numbers. Separate role: the smallest text score draws,
  //! so a pixel font has to land on its grid to stay legible.
  QFont RulerFont;

  //! Script and shader editors.
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

  //! Also called by load(), so a skin does not inherit the previous one's
  //! fonts.
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

//! Application font size before any skin has loaded; the "application" role
//! replaces it.
SCORE_LIB_BASE_EXPORT int uiFontSize() noexcept;

//! The font size every hardcoded pixel length in the widget UI was drawn
//! against.
inline constexpr int referenceFontSize = 13;

//! Current application font size over referenceFontSize.
SCORE_LIB_BASE_EXPORT double fontScale() noexcept;

//! A referenceFontSize-relative length in the current skin's terms, floored
//! at one pixel.
SCORE_LIB_BASE_EXPORT int scaledPixels(int px) noexcept;

//! scaledPixels() as a square.
SCORE_LIB_BASE_EXPORT QSize scaledIcon(int px) noexcept;

//! Runs \p f now and on every skin change. Needed for any size derived from
//! a font: the window and the toolbars are built before the first skin loads.
//!
//! Callable during that construction -- Skin::instance() needs the application
//! context, so the subscription is deferred by one event loop turn.
SCORE_LIB_BASE_EXPORT void onSkinChange(QObject* owner, std::function<void()> f);

//! onSkinChange(widget, setIconSize(scaledIcon(px))).
template <typename T>
void setSkinIconSize(T* widget, int px)
{
  onSkinChange(widget, [widget, px] { widget->setIconSize(scaledIcon(px)); });
}

//! Hinting for the fonts built before any skin has loaded. A skin sets it per
//! role, which wins over this.
SCORE_LIB_BASE_EXPORT QFont::HintingPreference uiFontHinting() noexcept;

//! NoSubpixelAntialias on macOS: QCocoaScreen rewrites Subpixel_None to
//! Subpixel_RGB, so the style strategy is the only way to get grayscale AA.
SCORE_LIB_BASE_EXPORT QFont::StyleStrategy uiFontStyleStrategy() noexcept;

//! Design grid of a pixel font score ships, 0 for anything else. These only
//! render sharply at whole multiples of it.
SCORE_LIB_BASE_EXPORT int pixelFontGrid(const QString& family) noexcept;

//! \p px rounded down to a multiple of \p f's grid, or up to one step when
//! smaller than that; unchanged for an outline font. Any computed font size
//! has to go through this.
SCORE_LIB_BASE_EXPORT int snapToFontGrid(const QFont& f, int px) noexcept;

//! setPixelSize through snapToFontGrid.
SCORE_LIB_BASE_EXPORT void setSnappedPixelSize(QFont& f, int px) noexcept;

//! Idempotent, and called by the Skin itself, so tests and alternate hosts
//! get the fonts without the application bootstrap.
SCORE_LIB_BASE_EXPORT void registerApplicationFonts();

//! Does not touch Skin::instance(), so it is callable before the application
//! context exists.
SCORE_LIB_BASE_EXPORT QFont defaultApplicationFont() noexcept;

//! False below Qt 6.6, or without QT_CONFIG(highdpiscaling): QHighDpiScaling
//! exists from 5.6 but only updates the screens from 6.5 and only emits the
//! QScreen signals from 6.6.
SCORE_LIB_BASE_EXPORT bool canSetGlobalScaleFactorLive() noexcept;

//! QT_SCALE_FACTOR, live, through Qt private API. Qt warns on the console
//! when windows are open; the factor still applies. Bumps the Skin and emits
//! changed() so the caches keyed on the device pixel ratio drop. False if the
//! factor is out of range or scaling is unavailable.
SCORE_LIB_BASE_EXPORT bool setGlobalScaleFactor(double factor);

//! Also reseeds the per-widget-class fonts the platform theme installs, which
//! would otherwise win. Must run after QApplication::setStyle(), which resets
//! that hash.
SCORE_LIB_BASE_EXPORT void setupApplicationFont(const QFont& f);

}
