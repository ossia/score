// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Skin.hpp"


#include <score/application/ApplicationContext.hpp>
#include <score/widgets/Pixmap.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <ossia/detail/flat_map.hpp>

// Live scale-factor changes need QHighDpiScaling, which is Qt private API.
// It has existed since Qt 5.6, but until 6.5 setGlobalFactor() only updated
// an internal field: it gained the screen updateGeometry() call in 6.5 and
// the QScreen change signals in 6.6. Below 6.6 the call is therefore a no-op
// as far as the UI is concerned, so we do not pretend otherwise and the
// setting keeps its "needs restart" behaviour. (Verified against qtbase tags:
// 6.2/6.3/6.4 have neither, 6.5 has updateGeometry, 6.6 adds UpdateEmitter.)
#if __has_include(<QtGui/private/qhighdpiscaling_p.h>) \
    && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <QtGui/private/qhighdpiscaling_p.h>
#define SCORE_HAS_LIVE_SCALE_FACTOR 1
#else
#define SCORE_HAS_LIVE_SCALE_FACTOR 0
#endif

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QDirIterator>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QWidget>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

#include <algorithm>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::Skin)

#define SCORE_INSERT_COLOR(Col) \
  {                             \
    #Col, &Col                  \
  }

#define SCORE_INSERT_COLOR_CUSTOM(Hex, Name) \
  Brush::Pair                                \
  {                                          \
    QStringLiteral(Name), QColor(Hex)        \
  }

namespace score
{
int uiFontSize() noexcept
{
  // Not a setting: this is only the size the application font has between
  // QApplication starting and the first skin loading, a few hundred
  // milliseconds of splash screen. The skin's "application" role replaces it
  // immediately afterwards.
  //
  // 13 px because QFont("Ubuntu", 10) is a *point* size, which is what the
  // rest of the UI is proportioned against, and 10 pt is 13 px at 96 DPI.
  if(qEnvironmentVariableIsSet("SCORE_SMOL_FONT"))
    return 11;
  return 13;
}

double fontScale() noexcept
{
  // The application's font rather than Skin::ApplicationFont, although the
  // two are kept equal: the main window is built before the application
  // context exists, and Skin::instance() needs that context. Widget sizes
  // are wanted exactly there.
  if(!qGuiApp)
    return 1.;

  // QFontInfo, not pixelSize(): a skin may name a point size, and then
  // pixelSize() is -1. QFontInfo resolves whichever of the two was set
  // against the screen the same way the painter will.
  const double px = QFontInfo{qGuiApp->font()}.pixelSize();
  if(px <= 0)
    return 1.;
  return px / double(referenceFontSize);
}

int scaledPixels(int px) noexcept
{
  return std::max(1, qRound(px * fontScale()));
}

QSize scaledIcon(int px) noexcept
{
  const int s = scaledPixels(px);
  return {s, s};
}

void onSkinChange(QObject* owner, std::function<void()> f)
{
  f();
  QTimer::singleShot(0, owner, [owner, f = std::move(f)] {
    QObject::connect(&Skin::instance(), &Skin::changed, owner, f);
    // A skin may well have loaded between the construction and this turn of
    // the event loop, and its changed() went to nobody.
    f();
  });
}

QFont::HintingPreference uiFontHinting() noexcept
{
  return QFont::PreferFullHinting;
}

QFont::StyleStrategy uiFontStyleStrategy() noexcept
{
  if(qEnvironmentVariableIsSet("SCORE_SMOL_FONT"))
  {
    return (QFont::StyleStrategy)(QFont::NoAntialias | QFont::PreferBitmap
                                  | QFont::PreferNoShaping);
  }
  else
  {
#if defined(__APPLE__)
    return QFont::NoSubpixelAntialias;
#else
    return QFont::PreferDefault;
#endif
  }
}

int pixelFontGrid(const QString& family) noexcept
{
  // Measured from the font outlines: every coordinate in these is a multiple
  // of unitsPerEm / grid. Kept next to the fonts they describe rather than in
  // the settings UI, since the skin generators assert on the same numbers.
  static const std::pair<QLatin1String, int> grids[]{
      {QLatin1String("Galmuri7"), 8},
      {QLatin1String("Galmuri9"), 10},
      {QLatin1String("Galmuri11"), 12},
      {QLatin1String("Galmuri14"), 15},
      {QLatin1String("GalmuriMono7"), 8},
      {QLatin1String("GalmuriMono9"), 10},
      {QLatin1String("GalmuriMono11"), 12},
      {QLatin1String("Departure Mono"), 11},
      // The bitmap is exact by construction. CozetteVector is a traced copy
      // whose 2048-unit em does not divide by 13, so its outlines cannot sit
      // exactly on the grid and adjacent glyphs occasionally lose their 1 px
      // gap; prefer "Cozette" and keep the vector for sizes above 13.
      {QLatin1String("Cozette"), 13},
      {QLatin1String("CozetteVector"), 13},
      {QLatin1String("Ark Pixel 10px Prop latin"), 10},
      {QLatin1String("Ark Pixel 12px Prop latin"), 12},
      {QLatin1String("Ark Pixel 16px Prop latin"), 16},
  };

  for(auto& [name, grid] : grids)
    if(family == name)
      return grid;
  return 0;
}

int snapToFontGrid(const QFont& f, int px) noexcept
{
  const auto& families = f.families();
  const int grid
      = pixelFontGrid(families.isEmpty() ? f.family() : families.constFirst());
  if(grid <= 0 || px <= 0)
    return px;

  // Down rather than to the nearest, so a label cannot grow out of a box that
  // was measured for it -- except below one step, where there is no smaller
  // size the font can be drawn at. Callers that shrink in a loop have to stop
  // when the result comes back no smaller than what they asked for.
  return std::max(1, px / grid) * grid;
}

void setSnappedPixelSize(QFont& f, int px) noexcept
{
  f.setPixelSize(snapToFontGrid(f, px));
}

void registerApplicationFonts()
{
  // Idempotent: skins, the application and the tests all want the fonts
  // present, and whichever runs first should be the one that pays for it.
  // Keyed on the application instance rather than a plain flag, because the
  // font database is per-QGuiApplication: a test that builds a second
  // application gets an empty one, and a process-wide flag would leave it
  // that way.
  static QCoreApplication* registeredFor = nullptr;
  if(!qApp || registeredFor == qApp)
    return;
  registeredFor = qApp;

  QDirIterator it(":/fonts", QDirIterator::Subdirectories);
  while(it.hasNext())
  {
    const auto font = it.next();
    if(font.endsWith("ttf", Qt::CaseInsensitive)
       || font.endsWith("otf", Qt::CaseInsensitive)
       || font.endsWith("bdf", Qt::CaseInsensitive))
    {
      QFontDatabase::addApplicationFont(font);
    }
  }
}

QFont defaultApplicationFont() noexcept
{
  registerApplicationFonts();

  QFont f{qEnvironmentVariableIsSet("SCORE_SMOL_FONT") ? "Departure Mono" : "Ubuntu"};
  f.setPixelSize(uiFontSize());
  f.setHintingPreference(uiFontHinting());
  f.setStyleStrategy(uiFontStyleStrategy());
  return f;
}

bool canSetGlobalScaleFactorLive() noexcept
{
#if SCORE_HAS_LIVE_SCALE_FACTOR && QT_CONFIG(highdpiscaling)
  return true;
#else
  return false;
#endif
}

bool setGlobalScaleFactor(double factor)
{
  if(!qGuiApp)
    return false;

  // Same bounds Application.cpp applies when it reads Skin/Zoom at startup.
  if(!(factor >= 1.0 && factor <= 10.0))
    return false;

#if SCORE_HAS_LIVE_SCALE_FACTOR && QT_CONFIG(highdpiscaling)
  QHighDpiScaling::setGlobalFactor(factor);

  // setGlobalFactor updates the screens, but a window that already exists
  // keeps the ratio it was created with, in both directions. Measured: after
  // setGlobalFactor(2) the screen reports 2.0 while the open window still
  // reports 1.0, and it only picks the new value up across a hide/show. This
  // is what Qt's "should only be called when no windows exist" warning is
  // about; cycling the top-level windows is what makes it actually apply.
  const auto windows = QApplication::topLevelWidgets();
  for(QWidget* w : windows)
  {
    // Actual windows only. topLevelWidgets() also returns dialogs, menus and
    // tooltips, and hiding a QDialog exits its exec() loop
    // (QDialogPrivate::setVisible) -- which would tear down the settings
    // dialog that the zoom control lives in, from under itself.
    if(!w->isVisible() || w->windowType() != Qt::Window)
      continue;

    // Some window managers drop a maximised or fullscreen window back to its
    // normal geometry across the cycle.
    const auto state = w->windowState();
    w->hide();
    w->show();
    if(w->windowState() != state)
      w->setWindowState(state);
  }

  // Every glyph image and pixmap in the UI was rasterised at the previous
  // ratio; score::newImage() bakes it in at creation. Bumping the Skin drops
  // the caches that key on LoadIndex and, through changed(), the ones that
  // clear on a skin change.
  Skin& skin = Skin::instance();
  skin.LoadIndex++;
  skin.changed();
  return true;
#else
  return false;
#endif
}

void setupApplicationFont(const QFont& f)
{
  if(!qGuiApp)
    return;

  qGuiApp->setFont(f);

  // The platform theme seeds per-class fonts which override the application
  // font; macOS provides most of this list, so set them explicitly.
  for(const char* widgetClass :
      {"QMenu", "QMenuBar", "QMenuItem", "QMessageBox", "QLabel", "QTipLabel",
       "QTitleBar", "QStatusBar", "QMdiSubWindowTitleBar", "QDockWidgetTitle",
       "QPushButton", "QCheckBox", "QRadioButton", "QToolButton", "QAbstractItemView",
       "QListView", "QHeaderView", "QListBox", "QComboMenuItem", "QComboLineEdit",
       "QSmallFont", "QMiniFont"})
  {
    QApplication::setFont(f, widgetClass);
  }
}

struct Skin::color_map
{
  explicit color_map(std::initializer_list<std::pair<QString, Brush*>> list)
      : left(list.begin(), list.end())
  {
    for(auto& pair : list)
    {
      right.insert({pair.second, pair.first});
    }
  }

  ossia::flat_map<QString, Brush*> left;
  ossia::flat_map<Brush*, QString> right;
};
Skin::~Skin()
{
  delete m_colorMap;
}
Skin::Skin() noexcept
    : TransparentPen{Qt::transparent}
    , TransparentBrush{Qt::transparent}
    , NoPen{Qt::NoPen}
    , NoBrush{Qt::NoBrush}
    , TextBrush{QColor("#1f2a30")}
    , m_colorMap{initColorMap()}
    , m_defaultPalette{
          SCORE_INSERT_COLOR_CUSTOM("#3F51B5", "Indigo"),
          SCORE_INSERT_COLOR_CUSTOM("#2196F3", "Blue"),
          SCORE_INSERT_COLOR_CUSTOM("#03A9F4", "LightBlue"),
          SCORE_INSERT_COLOR_CUSTOM("#00BCD4", "Cyan"),
          SCORE_INSERT_COLOR_CUSTOM("#009688", "Teal"),
          SCORE_INSERT_COLOR_CUSTOM("#4CAF50", "Green"),
          SCORE_INSERT_COLOR_CUSTOM("#8BC34A", "LightGreen"),
          SCORE_INSERT_COLOR_CUSTOM("#CDDC39", "Lime"),
          SCORE_INSERT_COLOR_CUSTOM("#FFEB3B", "Yellow"),
          SCORE_INSERT_COLOR_CUSTOM("#FFC107", "Amber"),
          SCORE_INSERT_COLOR_CUSTOM("#FF9800", "Orange"),
          SCORE_INSERT_COLOR_CUSTOM("#FF5722", "DeepOrange"),
          SCORE_INSERT_COLOR_CUSTOM("#F44336", "Red"),
          SCORE_INSERT_COLOR_CUSTOM("#E91E63", "Pink"),
          SCORE_INSERT_COLOR_CUSTOM("#9C27B0", "Purple"),
          SCORE_INSERT_COLOR_CUSTOM("#673AB7", "DeepPurple"),
          SCORE_INSERT_COLOR_CUSTOM("#455A64", "BlueGrey"),
          SCORE_INSERT_COLOR_CUSTOM("#9E9E9E", "Grey"),
          SCORE_INSERT_COLOR_CUSTOM("#FFFFFF", "White"),
          SCORE_INSERT_COLOR_CUSTOM("#000000", "Black")}
{
  setupFonts();

  // A skin may name a different application font; re-apply it when one loads.
  // Owned here rather than by the application, because Skin::instance() needs
  // the application context and the early font setup runs before that exists.
  // changed() is only emitted on skin load and on a theme edit, never by the
  // pulse timer, so this is not a per-frame cost.
  connect(this, &Skin::changed, this, [this] {
    if(qGuiApp && qGuiApp->font() != ApplicationFont)
      score::setupApplicationFont(ApplicationFont);
  });

  for(auto& c : m_defaultPalette)
  {
    m_colorMap->left.insert({c.first, &c.second});
    m_colorMap->right.insert({&c.second, c.first});
  }

  // make the "lighter" of black more light.
  {
    Brush& blackBrush = m_defaultPalette.back().second;
    blackBrush.lighter = Gray.main;
    blackBrush.lighter180 = HalfLight.main;
  }

  this->startTimer(32, Qt::CoarseTimer);

  SliderBrush = QColor{"#161514"};
  SliderPen = QPen{QColor{"#62400a"}, 1};
  SliderInteriorBrush = QColor{"#62400a"};
  SliderLine = QPen{QColor{"#c58014"}, 1, Qt::SolidLine, Qt::FlatCap};
  SliderTextPen = QColor{"#d0d0d0"};

  int hotspotX = 12;
  int hotspotY = 10;
  CursorPointer = score::get_cursor(":/icons/cursor_pointer.png", hotspotX, hotspotY);

  int centerHotspot = 16;
  CursorOpenHand
      = score::get_cursor(":/icons/cursor_open_hand.png", centerHotspot, centerHotspot);
  CursorClosedHand = score::get_cursor(
      ":/icons/cursor_closed_hand.png", centerHotspot, centerHotspot);
  CursorPointingHand = score::get_cursor(
      ":/icons/cursor_pointing_hand.png", centerHotspot, centerHotspot);

  int hotspot = 15;
  CursorMagnifier = score::get_cursor(":/icons/cursor_magnifier.png", hotspot, hotspot);
  CursorMove = score::get_cursor(":/icons/cursor_move.png", hotspot, hotspot);

  CursorScaleH
      = score::get_cursor(":/icons/cursor_scale_h.png", centerHotspot, centerHotspot);
  CursorScaleV
      = score::get_cursor(":/icons/cursor_scale_v.png", centerHotspot, centerHotspot);
  CursorScaleFDiag = score::get_cursor(
      ":/icons/cursor_scale_fdiag.png", centerHotspot, centerHotspot);

  CursorSpin
      = score::get_cursor(":/icons/cursor_spin.png", centerHotspot, centerHotspot);

  hotspotX = 12;
  hotspotY = 10;
  CursorPlayFromHere
      = score::get_cursor(":/icons/cursor_play_from_here.png", hotspotX, hotspotY);
  CursorCreationMode
      = score::get_cursor(":/icons/cursor_creation_mode.png", hotspotY, hotspotX);
}

std::vector<std::pair<const char*, QFont*>> Skin::fonts() noexcept
{
  return {
      {"application", &ApplicationFont},
      {"sans", &SansFont},
      {"sansSmall", &SansFontSmall},
      {"mono", &MonoFont},
      {"monoSmall", &MonoFontSmall},
      {"bold10", &Bold10Pt},
      {"bold12", &Bold12Pt},
      {"medium7", &Medium7Pt},
      {"medium8", &Medium8Pt},
      {"medium10", &Medium10Pt},
      {"medium12", &Medium12Pt},
      {"title", &TitleFont},
      {"sectionTitle", &SectionTitleFont},
      {"slider", &SliderFont},
      {"code", &CodeFont},
      {"timecode", &TimecodeFont}};
}

void Skin::setupFonts()
{
  registerApplicationFonts();

  SansFont = QFont{"Ubuntu"};
  MonoFont = QFont{"Courier Prime", int(10 * 96. / 72.), QFont::Black};
  MonoFontSmall = QFont{"Courier Prime", int(7 * 96. / 72.), QFont::Normal};
  SansFontSmall = QFont{"Ubuntu", int(7 * 96. / 72.)};

  MonoFont.setFamilies({"Courier Prime"});
  MonoFontSmall.setFamilies({"Ubuntu"});
  MonoFont.setFixedPitch(true);

  for(QFont* font : {&SansFont, &SansFontSmall, &MonoFont, &MonoFontSmall})
  {
    font->setStyleStrategy(
        QFont::StyleStrategy(QFont::ForceOutline | uiFontStyleStrategy()));
    font->setHintingPreference(uiFontHinting());
  }

  Bold10Pt = SansFont;
  Bold10Pt.setPixelSize(10 * 96. / 72.);
  Bold10Pt.setBold(true);

  Bold12Pt = Bold10Pt;
  Bold12Pt.setPixelSize(12 * 96. / 72.);

  Medium7Pt = SansFont;
  Medium7Pt.setPixelSize(7 * 96. / 72.);

  Medium8Pt = SansFont;
  Medium8Pt.setPixelSize(8 * 96. / 72.);

  Medium10Pt = SansFont;
  Medium10Pt.setPixelSize(10 * 96. / 72.);

  Medium12Pt = SansFont;
  Medium12Pt.setPixelSize(12 * 96. / 72.);

  TitleFont = SansFont;
  TitleFont.setPixelSize(14);
  TitleFont.setBold(true);

  // A heading over an inspector page: bold, one step under the panel title.
  SectionTitleFont = SansFont;
  SectionTitleFont.setPixelSize(12);
  SectionTitleFont.setBold(true);

  SliderFont = SansFont;
  SliderFont.setPixelSize(10 * 96. / 72.);
  SliderFont.setWeight(QFont::DemiBold);

  // The transport readout, proportioned as 18 pt, which is 24 px at 96 DPI.
  TimecodeFont = QFont{"Ubuntu"};
  TimecodeFont.setPixelSize(24);
  TimecodeFont.setWeight(QFont::DemiBold);

  // Script and shader editors. Vertical hinting only: code is read in
  // columns, and full hinting shifts glyphs off them.
  CodeFont = QFont{"IBM Plex Mono"};
  CodeFont.setPixelSize(13);
  CodeFont.setFixedPitch(true);
  CodeFont.setHintingPreference(QFont::PreferVerticalHinting);

  ApplicationFont = defaultApplicationFont();

  std::initializer_list<QFont*> mono_fonts = {&MonoFont, &MonoFontSmall};
  std::initializer_list<QFont*> fonts = {
      &SansFont,  &MonoFont,  &MonoFontSmall, &SansFontSmall, &Bold10Pt,   &Bold12Pt,
      &Medium7Pt, &Medium8Pt, &Medium10Pt,    &Medium12Pt,    &SliderFont, &TitleFont,
      &SectionTitleFont};
  for(QFont* font : fonts)
  {
    font->setHintingPreference(uiFontHinting());
    font->setStyleHint(QFont::StyleHint::SansSerif);
    font->setStyleStrategy(QFont::StyleStrategy(
        QFont::StyleStrategy::PreferQuality | QFont::StyleStrategy::PreferMatch
        | QFont::StyleStrategy::NoFontMerging | uiFontStyleStrategy()));
  }
  for(QFont* font : mono_fonts)
  {
    font->setStyleHint(QFont::StyleHint::Monospace);
  }
}

Skin::Skin(Skin::NoGUI)
    : m_colorMap{initColorMap()}
{
}

Skin::color_map* Skin::initColorMap() noexcept
{
  return new Skin::color_map{
      SCORE_INSERT_COLOR(Dark),           SCORE_INSERT_COLOR(HalfDark),
      SCORE_INSERT_COLOR(DarkGray),       SCORE_INSERT_COLOR(Gray),
      SCORE_INSERT_COLOR(LightGray),      SCORE_INSERT_COLOR(HalfLight),
      SCORE_INSERT_COLOR(Light),          SCORE_INSERT_COLOR(Emphasis1),
      SCORE_INSERT_COLOR(Emphasis2),      SCORE_INSERT_COLOR(Emphasis3),
      SCORE_INSERT_COLOR(Emphasis4),      SCORE_INSERT_COLOR(Emphasis5),
      SCORE_INSERT_COLOR(Base1),          SCORE_INSERT_COLOR(Base2),
      SCORE_INSERT_COLOR(Base3),          SCORE_INSERT_COLOR(Base4),
      SCORE_INSERT_COLOR(Base5),          SCORE_INSERT_COLOR(Warn1),
      SCORE_INSERT_COLOR(Warn2),          SCORE_INSERT_COLOR(Warn3),
      SCORE_INSERT_COLOR(Background1),    SCORE_INSERT_COLOR(Background2),
      SCORE_INSERT_COLOR(Transparent1),   SCORE_INSERT_COLOR(Transparent2),
      SCORE_INSERT_COLOR(Transparent3),   SCORE_INSERT_COLOR(Smooth1),
      SCORE_INSERT_COLOR(Smooth2),        SCORE_INSERT_COLOR(Smooth3),
      SCORE_INSERT_COLOR(Tender1),        SCORE_INSERT_COLOR(Tender2),
      SCORE_INSERT_COLOR(Tender3),        SCORE_INSERT_COLOR(Cable1),
      SCORE_INSERT_COLOR(Cable2),         SCORE_INSERT_COLOR(Cable3),
      SCORE_INSERT_COLOR(SelectedCable1), SCORE_INSERT_COLOR(SelectedCable2),
      SCORE_INSERT_COLOR(SelectedCable3), SCORE_INSERT_COLOR(Port1),
      SCORE_INSERT_COLOR(Port2),          SCORE_INSERT_COLOR(Port3),
      SCORE_INSERT_COLOR(Pulse1),         SCORE_INSERT_COLOR(Pulse2)};
}

Skin& score::Skin::instance() noexcept
{
  static const auto s = score::AppContext().applicationSettings.gui
                            ? std::unique_ptr<Skin>(new Skin())
                            : std::unique_ptr<Skin>(new Skin(Skin::NoGUI{}));
  return *s;
}

#define SCORE_CONVERT_COLOR(Col) \
  do                             \
  {                              \
    fromColor(#Col, Col);        \
  } while(0)
void Skin::load(const QJsonObject& obj, int parts)
{
  if(parts & Fonts)
  {
    // Reset first: a skin that names no fonts must get the built-in ones,
    // not whatever the skin before it set.
    setupFonts();
    loadFonts(obj["fonts"].toObject());
  }

  if(!(parts & Colours))
  {
    LoadIndex++;
    changed();
    return;
  }

  auto fromColor = [&](const QString& key, Brush& col) {
    auto arr = obj[key].toArray();
    if(arr.size() == 3)
      col = QColor(arr[0].toInt(), arr[1].toInt(), arr[2].toInt());
    else if(arr.size() == 4)
      col = QColor(arr[0].toInt(), arr[1].toInt(), arr[2].toInt(), arr[3].toInt());
  };

  SCORE_CONVERT_COLOR(Dark);
  SCORE_CONVERT_COLOR(HalfDark);
  SCORE_CONVERT_COLOR(DarkGray);
  SCORE_CONVERT_COLOR(Gray);
  SCORE_CONVERT_COLOR(LightGray);
  SCORE_CONVERT_COLOR(HalfLight);
  SCORE_CONVERT_COLOR(Light);

  SCORE_CONVERT_COLOR(Emphasis1);
  SCORE_CONVERT_COLOR(Emphasis2);
  SCORE_CONVERT_COLOR(Emphasis3);
  SCORE_CONVERT_COLOR(Emphasis4);
  SCORE_CONVERT_COLOR(Emphasis5);

  SCORE_CONVERT_COLOR(Base1);
  SCORE_CONVERT_COLOR(Base2);
  SCORE_CONVERT_COLOR(Base3);
  SCORE_CONVERT_COLOR(Base4);
  SCORE_CONVERT_COLOR(Base5);

  SCORE_CONVERT_COLOR(Warn1);
  SCORE_CONVERT_COLOR(Warn2);
  SCORE_CONVERT_COLOR(Warn3);

  SCORE_CONVERT_COLOR(Background1);
  SCORE_CONVERT_COLOR(Background2);

  SCORE_CONVERT_COLOR(Transparent1);
  SCORE_CONVERT_COLOR(Transparent2);
  SCORE_CONVERT_COLOR(Transparent3);

  SCORE_CONVERT_COLOR(Smooth1);
  SCORE_CONVERT_COLOR(Smooth2);
  SCORE_CONVERT_COLOR(Smooth3);

  SCORE_CONVERT_COLOR(Tender1);
  SCORE_CONVERT_COLOR(Tender2);
  SCORE_CONVERT_COLOR(Tender3);

  SCORE_CONVERT_COLOR(Cable1);
  SCORE_CONVERT_COLOR(Cable2);
  SCORE_CONVERT_COLOR(Cable3);
  SCORE_CONVERT_COLOR(SelectedCable1);
  SCORE_CONVERT_COLOR(SelectedCable2);
  SCORE_CONVERT_COLOR(SelectedCable3);
  SCORE_CONVERT_COLOR(Port1);
  SCORE_CONVERT_COLOR(Port2);
  SCORE_CONVERT_COLOR(Port3);

  SCORE_CONVERT_COLOR(Pulse1);
  SCORE_CONVERT_COLOR(Pulse2);

  // make the "lighter" of black more light.
  {
    Transparent1.darker = Transparent1.lighter180;
    Transparent1.darker300 = Transparent1.lighter;
    Transparent1.lighter = Gray.main;
    Transparent1.lighter180 = HalfLight.main;
  }

  LoadIndex++;
  changed();
}

static QFont::HintingPreference hintingFromString(
    const QString& v, QFont::HintingPreference fallback) noexcept
{
  if(v == "None")
    return QFont::PreferNoHinting;
  if(v == "Vertical")
    return QFont::PreferVerticalHinting;
  if(v == "Full")
    return QFont::PreferFullHinting;
  if(v == "Default")
    return QFont::PreferDefaultHinting;
  return fallback;
}

static QString hintingToString(QFont::HintingPreference h) noexcept
{
  switch(h)
  {
    case QFont::PreferNoHinting:
      return QStringLiteral("None");
    case QFont::PreferVerticalHinting:
      return QStringLiteral("Vertical");
    case QFont::PreferFullHinting:
      return QStringLiteral("Full");
    default:
      return QStringLiteral("Default");
  }
}

void Skin::loadFonts(const QJsonObject& spec_obj)
{
  if(spec_obj.isEmpty())
    return;

  // A "defaults" entry applies to every font the skin does not mention, so a
  // skin can switch the whole UI to one family without repeating itself.
  const QJsonObject defaults = spec_obj["defaults"].toObject();

  auto apply = [](QFont& font, const QJsonObject& spec) {
    if(spec.isEmpty())
      return;

    if(const auto fam = spec["families"].toArray(); !fam.isEmpty())
    {
      QStringList families;
      for(const auto& f : fam)
        families.push_back(f.toString());
      font.setFamilies(families);
    }
    else if(const auto f = spec["family"].toString(); !f.isEmpty())
    {
      font.setFamilies({f});
    }

    // Pixel size, not point size: a pixel font is only sharp at a whole
    // multiple of its design grid, and points go through DPI first.
    if(const auto px = spec["pixelSize"].toInt(); px > 0)
      font.setPixelSize(px);
    else if(const auto pt = spec["pointSize"].toInt(); pt > 0)
      font.setPointSize(pt);

    if(const auto s = spec["styleName"].toString(); !s.isEmpty())
      font.setStyleName(s);

    if(spec.contains("bold"))
      font.setBold(spec["bold"].toBool());
    if(spec.contains("fixedPitch"))
      font.setFixedPitch(spec["fixedPitch"].toBool());

    // Absolute, in pixels, so it does not scale with the size the way
    // percentage spacing does: these fonts are drawn on a fixed cell.
    if(spec.contains("letterSpacing"))
      font.setLetterSpacing(
          QFont::AbsoluteSpacing, spec["letterSpacing"].toDouble());
    if(spec.contains("italic"))
      font.setItalic(spec["italic"].toBool());
    if(const auto w = spec["weight"].toInt(); w > 0)
      font.setWeight(QFont::Weight(w));

    if(spec.contains("hinting"))
      font.setHintingPreference(
          hintingFromString(spec["hinting"].toString(), font.hintingPreference()));

    // Antialiasing is a bit in the style strategy, so flip it rather than
    // replacing the strategy and losing ForceOutline / NoFontMerging.
    if(spec.contains("antialias"))
    {
      auto strategy = int(font.styleStrategy());
      if(spec["antialias"].toBool())
        strategy &= ~int(QFont::NoAntialias);
      else
        strategy |= int(QFont::NoAntialias);
      font.setStyleStrategy(QFont::StyleStrategy(strategy));
    }
  };

  for(auto& [key, font] : fonts())
  {
    apply(*font, defaults);
    apply(*font, spec_obj[QLatin1String(key)].toObject());
  }
}

QJsonObject Skin::saveFonts() const
{
  QJsonObject fonts;
  for(auto& [key, font] : const_cast<Skin*>(this)->fonts())
  {
    QJsonObject spec;
    const auto families = font->families();
    if(families.size() > 1)
    {
      // A fallback chain has to be written back as one, or saving a skin that
      // has one collapses it to its first entry.
      QJsonArray arr;
      for(const auto& f : families)
        arr.push_back(f);
      spec["families"] = arr;
    }
    else
    {
      spec["family"] = families.empty() ? font->family() : families.front();
    }
    // Only fonts that were given an explicit pixel size get one written back.
    // Writing a computed height instead would pin a font that was deliberately
    // left to the point size, and save/load would not be a no-op.
    if(font->pixelSize() > 0)
      spec["pixelSize"] = font->pixelSize();
    else if(font->pointSize() > 0)
      spec["pointSize"] = font->pointSize();
    spec["bold"] = font->bold();
    spec["italic"] = font->italic();
    // Written either way: setupFonts() marks mono and code fixed-pitch, and
    // load() runs it before applying the file, so leaving the key out would
    // put the flag back rather than clear it.
    spec["fixedPitch"] = font->fixedPitch();
    if(font->letterSpacingType() == QFont::AbsoluteSpacing
       && font->letterSpacing() != 0.)
      spec["letterSpacing"] = font->letterSpacing();
    spec["weight"] = int(font->weight());
    spec["hinting"] = hintingToString(font->hintingPreference());
    spec["antialias"] = !(int(font->styleStrategy()) & int(QFont::NoAntialias));
    if(const auto s = font->styleName(); !s.isEmpty())
      spec["styleName"] = s;
    fonts[QLatin1String(key)] = spec;
  }
  return fonts;
}

QJsonObject Skin::toJson() const
{
  QJsonObject obj;
  for(auto& col : getColors())
  {
    obj.insert(
        col.second, QJsonArray{col.first.red(), col.first.green(), col.first.blue()});
  }
  obj["fonts"] = saveFonts();
  return obj;
}

#define SCORE_MAKE_PAIR_COLOR(Col) \
  vec.push_back(qMakePair(Col.color(), QStringLiteral(#Col)));
QVector<QPair<QColor, QString>> Skin::getColors() const
{
  QVector<QPair<QColor, QString>> vec;
  vec.reserve(27);

  SCORE_MAKE_PAIR_COLOR(Dark);
  SCORE_MAKE_PAIR_COLOR(HalfDark);
  SCORE_MAKE_PAIR_COLOR(DarkGray);
  SCORE_MAKE_PAIR_COLOR(Gray);
  SCORE_MAKE_PAIR_COLOR(LightGray);
  SCORE_MAKE_PAIR_COLOR(HalfLight);
  SCORE_MAKE_PAIR_COLOR(Light);
  SCORE_MAKE_PAIR_COLOR(Emphasis1);
  SCORE_MAKE_PAIR_COLOR(Emphasis2);
  SCORE_MAKE_PAIR_COLOR(Emphasis3);
  SCORE_MAKE_PAIR_COLOR(Emphasis4);
  SCORE_MAKE_PAIR_COLOR(Emphasis5);
  SCORE_MAKE_PAIR_COLOR(Base1);
  SCORE_MAKE_PAIR_COLOR(Base2);
  SCORE_MAKE_PAIR_COLOR(Base3);
  SCORE_MAKE_PAIR_COLOR(Base4);
  SCORE_MAKE_PAIR_COLOR(Base5);
  SCORE_MAKE_PAIR_COLOR(Warn1);
  SCORE_MAKE_PAIR_COLOR(Warn2);
  SCORE_MAKE_PAIR_COLOR(Warn3);
  SCORE_MAKE_PAIR_COLOR(Background1);
  SCORE_MAKE_PAIR_COLOR(Background2);
  SCORE_MAKE_PAIR_COLOR(Transparent1);
  SCORE_MAKE_PAIR_COLOR(Transparent2);
  SCORE_MAKE_PAIR_COLOR(Transparent3);
  SCORE_MAKE_PAIR_COLOR(Smooth1);
  SCORE_MAKE_PAIR_COLOR(Smooth2);
  SCORE_MAKE_PAIR_COLOR(Smooth3);
  SCORE_MAKE_PAIR_COLOR(Tender1);
  SCORE_MAKE_PAIR_COLOR(Tender2);
  SCORE_MAKE_PAIR_COLOR(Tender3);
  SCORE_MAKE_PAIR_COLOR(Cable1);
  SCORE_MAKE_PAIR_COLOR(Cable2);
  SCORE_MAKE_PAIR_COLOR(Cable3);
  SCORE_MAKE_PAIR_COLOR(SelectedCable1);
  SCORE_MAKE_PAIR_COLOR(SelectedCable2);
  SCORE_MAKE_PAIR_COLOR(SelectedCable3);
  SCORE_MAKE_PAIR_COLOR(Port1);
  SCORE_MAKE_PAIR_COLOR(Port2);
  SCORE_MAKE_PAIR_COLOR(Port3);
  SCORE_MAKE_PAIR_COLOR(Pulse1);
  SCORE_MAKE_PAIR_COLOR(Pulse2);

  return vec;
}
QVector<QPair<QColor, QString>> Skin::getDefaultPaletteColors() const
{
  QVector<QPair<QColor, QString>> vec_color;
  vec_color.reserve(18);

  for(auto& c : m_defaultPalette)
    vec_color.push_back({c.second.color(), c.first});

  return vec_color;
}

static bool pulse(QBrush& ref, bool pulse, int& idx)
{
  bool invert = false;
  auto col = ref.color();
  auto alpha = col.alphaF();
  if(pulse)
  {
    alpha += 0.02f;
    idx++;
    if(alpha >= 1.0f || idx > 24)
    {
      invert = true;
      alpha = 1;
      idx = 24;
    }
    col.setAlphaF(alpha);
  }
  else
  {
    alpha -= 0.02f;
    idx--;
    if(alpha <= 0.5f || idx < 0)
    {
      invert = true;
      alpha = 0.5f;
      idx = 0;
    }
    col.setAlphaF(alpha);
  }
  ref.setColor(col);
  return invert;
}

void Skin::timerEvent(QTimerEvent* event)
{
  int tmp{12};
  pulse(Pulse1.main.brush, m_pulseDirection, this->PulseIndex);
  auto invert = pulse(Pulse2.main.brush, m_pulseDirection, tmp);
  if(invert)
    m_pulseDirection = !m_pulseDirection;
}

const Brush* Skin::fromString(const QString& s) const
{
  auto it = m_colorMap->left.find(s);
  return it != m_colorMap->left.end() ? it->second : nullptr;
}

Brush* Skin::fromString(const QString& s)
{
  auto it = m_colorMap->left.find(s);
  return it != m_colorMap->left.end() ? it->second : nullptr;
}

QString Skin::toString(const Brush* c) const
{
  auto it = m_colorMap->right.find(const_cast<Brush*>(c));
  return it != m_colorMap->right.end() ? it->second : nullptr;
}

void Brush::reload(QColor color) noexcept
{
  *this = Brush{QBrush{color}};
}

Brush::Brush() noexcept { }
Brush::Brush(const Brush& other) noexcept
    : main{other.main}
    , darker{other.darker}
    , darker300{other.darker300}
    , lighter{other.lighter}
    , lighter180{other.lighter180}
{
}
Brush::Brush(Brush&& other) noexcept
    : main{other.main}
    , darker{other.darker}
    , darker300{other.darker300}
    , lighter{other.lighter}
    , lighter180{other.lighter180}
{
}
Brush& Brush::operator=(const Brush& other) noexcept
{
  main = other.main;
  darker = other.darker;
  darker300 = other.darker300;
  lighter = other.lighter;
  lighter180 = other.lighter180;

  return *this;
}
Brush& Brush::operator=(Brush&& other) noexcept
{
  main = other.main;
  darker = other.darker;
  darker300 = other.darker300;
  lighter = other.lighter;
  lighter180 = other.lighter180;
  return *this;
}
Brush::~Brush() = default;

Brush::Brush(const QBrush& b) noexcept
    : main{b}
    , darker{b.color().darker()}
    , darker300{b.color().darker(150)}
    , lighter{b.color().lighter()}
    , lighter180{b.color().lighter(180)}
{
}

Brush& Brush::operator=(const QBrush& b) noexcept
{
  main = b;
  darker = b.color().darker();
  darker300 = b.color().darker(150);
  lighter = b.color().lighter();
  lighter180 = b.color().lighter(180);
  return *this;
}

BrushSet::BrushSet() noexcept { }

BrushSet::BrushSet(const BrushSet& other) noexcept
    : brush{other.brush}
    , pen_cosmetic{other.pen_cosmetic}
    , pen0{other.pen0}
    , pen0_solid_round{other.pen0_solid_round}
    , pen1{other.pen1}
    , pen1_dotted{other.pen1_dotted}
    , pen1_solid_flat_miter{other.pen1_solid_flat_miter}
    , pen1_5{other.pen1_5}
    , pen2{other.pen2}
    , pen2_solid_round_round{other.pen2_solid_round_round}
    , pen2_solid_flat_miter{other.pen2_solid_flat_miter}
    , pen2_dashdot_square_miter{other.pen2_dashdot_square_miter}
    , pen2_dotted_square_miter{other.pen2_dotted_square_miter}
    , pen3{other.pen3}
    , pen3_solid_flat_miter{other.pen3_solid_flat_miter}
    , pen3_solid_round_round{other.pen3_solid_round_round}
    , pen3_dashed_flat_miter{other.pen3_dashed_flat_miter}
{
}

BrushSet::BrushSet(BrushSet&& other) noexcept
    : brush{other.brush}
    , pen_cosmetic{other.pen_cosmetic}
    , pen0{other.pen0}
    , pen0_solid_round{other.pen0_solid_round}
    , pen1{other.pen1}
    , pen1_dotted{other.pen1_dotted}
    , pen1_solid_flat_miter{other.pen1_solid_flat_miter}
    , pen1_5{other.pen1_5}
    , pen2{other.pen2}
    , pen2_solid_round_round{other.pen2_solid_round_round}
    , pen2_solid_flat_miter{other.pen2_solid_flat_miter}
    , pen2_dashdot_square_miter{other.pen2_dashdot_square_miter}
    , pen2_dotted_square_miter{other.pen2_dotted_square_miter}
    , pen3{other.pen3}
    , pen3_solid_flat_miter{other.pen3_solid_flat_miter}
    , pen3_solid_round_round{other.pen3_solid_round_round}
    , pen3_dashed_flat_miter{other.pen3_dashed_flat_miter}
{
}

BrushSet& BrushSet::operator=(const BrushSet& other) noexcept
{
  brush = other.brush;
  pen_cosmetic = other.pen_cosmetic;
  pen0 = other.pen0;
  pen0_solid_round = other.pen0_solid_round;
  pen1 = other.pen1;
  pen1_dotted = other.pen1_dotted;
  pen1_solid_flat_miter = other.pen1_solid_flat_miter;
  pen1_5 = other.pen1_5;
  pen2 = other.pen2;
  pen2_solid_round_round = other.pen2_solid_round_round;
  pen2_solid_flat_miter = other.pen2_solid_flat_miter;
  pen2_dashdot_square_miter = other.pen2_dashdot_square_miter;
  pen2_dotted_square_miter = other.pen2_dotted_square_miter;
  pen3 = other.pen3;
  pen3_solid_flat_miter = other.pen3_solid_flat_miter;
  pen3_solid_round_round = other.pen3_solid_round_round;
  pen3_dashed_flat_miter = other.pen3_dashed_flat_miter;
  return *this;
}

BrushSet& BrushSet::operator=(BrushSet&& other) noexcept
{
  brush = other.brush;
  pen_cosmetic = other.pen_cosmetic;
  pen0 = other.pen0;
  pen0_solid_round = other.pen0_solid_round;
  pen1 = other.pen1;
  pen1_dotted = other.pen1_dotted;
  pen1_solid_flat_miter = other.pen1_solid_flat_miter;
  pen1_5 = other.pen1_5;
  pen2 = other.pen2;
  pen2_solid_round_round = other.pen2_solid_round_round;
  pen2_solid_flat_miter = other.pen2_solid_flat_miter;
  pen2_dashdot_square_miter = other.pen2_dashdot_square_miter;
  pen2_dotted_square_miter = other.pen2_dotted_square_miter;
  pen3 = other.pen3;
  pen3_solid_flat_miter = other.pen3_solid_flat_miter;
  pen3_solid_round_round = other.pen3_solid_round_round;
  pen3_dashed_flat_miter = other.pen3_dashed_flat_miter;
  return *this;
}

BrushSet::~BrushSet() { }

BrushSet::BrushSet(const QBrush& b) noexcept
    : brush{b}
{
  setupPens();
}

BrushSet& BrushSet::operator=(const QBrush& b) noexcept
{
  brush = b;
  setupPens();
  return *this;
}

void BrushSet::setupPens()
{
  pen_cosmetic.setBrush(brush);
  pen_cosmetic.setCosmetic(true);

  pen0.setBrush(brush);
  pen0.setWidth(0);

  pen0_solid_round.setBrush(brush);
  pen0_solid_round.setWidth(0);
  pen0_solid_round.setCapStyle(Qt::RoundCap);
  pen0_solid_round.setJoinStyle(Qt::RoundJoin);

  pen1.setBrush(brush);
  pen1.setWidth(1);

  pen1_dotted.setBrush(brush);
  pen1_dotted.setWidthF(1.5);
  pen1_dotted.setStyle(Qt::DotLine);

  pen1_solid_flat_miter.setBrush(brush);
  pen1_solid_flat_miter.setWidth(1);
  pen1_solid_flat_miter.setCapStyle(Qt::FlatCap);
  pen1_solid_flat_miter.setJoinStyle(Qt::MiterJoin);

  pen1_5.setBrush(brush);
  pen1_5.setWidthF(1.5);

  pen2.setBrush(brush);
  pen2.setWidth(2);

  pen2_solid_round_round.setBrush(brush);
  pen2_solid_round_round.setWidth(2);
  pen2_solid_round_round.setCapStyle(Qt::RoundCap);
  pen2_solid_round_round.setJoinStyle(Qt::RoundJoin);

  pen2_solid_flat_miter.setBrush(brush);
  pen2_solid_flat_miter.setWidth(2);
  pen2_solid_flat_miter.setCapStyle(Qt::FlatCap);
  pen2_solid_flat_miter.setJoinStyle(Qt::MiterJoin);

  pen2_dashdot_square_miter.setBrush(brush);
  pen2_dashdot_square_miter.setWidth(2);
  pen2_dashdot_square_miter.setStyle(Qt::DashDotLine);
  pen2_dashdot_square_miter.setCapStyle(Qt::FlatCap);
  pen2_dashdot_square_miter.setJoinStyle(Qt::MiterJoin);

  pen2_dotted_square_miter.setBrush(brush);
  pen2_dotted_square_miter.setWidth(2);
  pen2_dotted_square_miter.setStyle(Qt::DotLine);
  pen2_dotted_square_miter.setCapStyle(Qt::SquareCap);
  pen2_dotted_square_miter.setJoinStyle(Qt::MiterJoin);

  pen3.setBrush(brush);
  pen3.setWidth(3);

  pen3_solid_round_round.setBrush(brush);
  pen3_solid_round_round.setWidth(3);
  pen3_solid_round_round.setCapStyle(Qt::RoundCap);
  pen3_solid_round_round.setJoinStyle(Qt::RoundJoin);

  pen3_solid_flat_miter.setBrush(brush);
  pen3_solid_flat_miter.setWidth(3);
  pen3_solid_flat_miter.setCapStyle(Qt::FlatCap);
  pen3_solid_flat_miter.setJoinStyle(Qt::MiterJoin);

  pen3_dashed_flat_miter.setBrush(brush);
  pen3_dashed_flat_miter.setWidth(3);
  pen3_dashed_flat_miter.setStyle(Qt::CustomDashLine);
  pen3_dashed_flat_miter.setDashPattern({2., 4.});
  pen3_dashed_flat_miter.setCapStyle(Qt::FlatCap);
  pen3_dashed_flat_miter.setJoinStyle(Qt::MiterJoin);
}

#undef SCORE_INSERT_COLOR
#undef SCORE_CONVERT_COLOR
#undef SCORE_MAKE_PAIR_COLOR
}
