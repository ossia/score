// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Skin.hpp"

#include <score/application/ApplicationContext.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <score/widgets/Pixmap.hpp>

#include <ossia/detail/flat_map.hpp>

#include <boost/assign/list_of.hpp>

#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QBrush>
#include <QColor>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::Skin)

#define SCORE_INSERT_COLOR(Col) \
  {                             \
#Col, &Col                  \
  }

#define SCORE_INSERT_COLOR_CUSTOM(Hex, Name) Brush::Pair{QStringLiteral(Name), QColor(Hex)}

namespace score
{
struct Skin::color_map
{
  color_map(std::initializer_list<std::pair<QString, Brush*>> list)
      : left(list.begin(), list.end())
  {
    for (auto& pair : list)
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
    : SansFont{"Ubuntu"}
    , MonoFont{"APCCourier-Bold", int(10 * 96. / 72.), QFont::Black}
    , MonoFontSmall{"APCCourier-Bold", int(7 * 96. / 72.), QFont::Normal}
    , SansFontSmall{"Ubuntu", int(7 * 96. / 72.)}
    , TransparentPen{Qt::transparent}
    , TransparentBrush{Qt::transparent}
    , NoPen{Qt::NoPen}
    , NoBrush{Qt::NoBrush}
    , TextBrush{QColor("#1f2a30")}
    , m_colorMap(
          new color_map{SCORE_INSERT_COLOR(Dark),           SCORE_INSERT_COLOR(HalfDark),
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
                        SCORE_INSERT_COLOR(Pulse1),         SCORE_INSERT_COLOR(Pulse2)})
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
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
  MonoFont.setFamilies({"APCCourier-Bold"});
  MonoFontSmall.setFamilies({"Ubuntu"});
#endif

  for (QFont* font : {&SansFont, &SansFontSmall, &MonoFont, &MonoFontSmall})
  {
    font->setStyleStrategy(QFont::ForceOutline);
    font->setHintingPreference(QFont::PreferVerticalHinting);
  }

  for (auto& c : m_defaultPalette)
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

  SliderBrush = QColor{"#161514"};
  SliderPen = QPen{QColor{"#62400a"}, 1};
  SliderInteriorBrush = QColor{"#62400a"};
  SliderLine = QPen{QColor{"#c58014"}, 1, Qt::SolidLine, Qt::FlatCap};
  SliderTextPen = QColor{"#d0d0d0"};
  SliderFont = SansFont;
  SliderFont.setPixelSize(10 * 96. / 72.);
  SliderFont.setWeight(QFont::DemiBold);

#if defined(__linux__)
  const double dpr = qApp->devicePixelRatio();
#else
  constexpr double dpr = 1.;
#endif

  int hotspotX = 12 * dpr;
  int hotspotY = 10 * dpr;
  CursorPointer = QCursor{score::get_pixmap(":/icons/cursor_pointer.png"), hotspotX, hotspotY};

  int centerHotspot = 16 * dpr;
  CursorOpenHand
      = QCursor{score::get_pixmap(":/icons/cursor_open_hand.png"), centerHotspot, centerHotspot};
  CursorClosedHand
      = QCursor{score::get_pixmap(":/icons/cursor_closed_hand.png"), centerHotspot, centerHotspot};
  CursorPointingHand = QCursor{
      score::get_pixmap(":/icons/cursor_pointing_hand.png"), centerHotspot, centerHotspot};

  int hotspot = 15 * dpr;
  CursorMagnifier = QCursor{score::get_pixmap(":/icons/cursor_magnifier.png"), hotspot, hotspot};
  CursorMove = QCursor{score::get_pixmap(":/icons/cursor_move.png"), hotspot, hotspot};

  CursorScaleH
      = QCursor{score::get_pixmap(":/icons/cursor_scale_h.png"), centerHotspot, centerHotspot};
  CursorScaleV
      = QCursor{score::get_pixmap(":/icons/cursor_scale_v.png"), centerHotspot, centerHotspot};
  CursorScaleFDiag
      = QCursor{score::get_pixmap(":/icons/cursor_scale_fdiag.png"), centerHotspot, centerHotspot};

  CursorSpin
      = QCursor{score::get_pixmap(":/icons/cursor_spin.png"), centerHotspot, centerHotspot};

  hotspotX = 12 * dpr;
  hotspotY = 10 * dpr;
  CursorPlayFromHere
      = QCursor{score::get_pixmap(":/icons/cursor_play_from_here.png"), hotspotX, hotspotY};
  CursorCreationMode
      = QCursor{score::get_pixmap(":/icons/cursor_creation_mode.png"), hotspotY, hotspotX};

  std::initializer_list<QFont*> mono_fonts = {&MonoFont, &MonoFontSmall};
  std::initializer_list<QFont*> fonts
      = {&SansFont,
         &MonoFont,
         &MonoFontSmall,
         &SansFontSmall,
         &Bold10Pt,
         &Bold12Pt,
         &Medium7Pt,
         &Medium8Pt,
         &Medium10Pt,
         &Medium12Pt,
         &SliderFont,
         &TitleFont};
  for (QFont* font : fonts)
  {
    font->setHintingPreference(QFont::HintingPreference::PreferVerticalHinting);
    font->setStyleHint(QFont::StyleHint::SansSerif);
    font->setStyleStrategy(QFont::StyleStrategy(
                             QFont::StyleStrategy::PreferQuality |
                             QFont::StyleStrategy::PreferMatch |
                             QFont::StyleStrategy::NoFontMerging)
    );
  }
  for (QFont* font : mono_fonts)
  {
    font->setStyleHint(QFont::StyleHint::Monospace);
  }
}

Skin::Skin(Skin::NoGUI)
  : m_colorMap{new color_map{}}
{

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
  } while (0)
void Skin::load(const QJsonObject& obj)
{
  auto fromColor = [&](const QString& key, Brush& col) {
    auto arr = obj[key].toArray();
    if (arr.size() == 3)
      col = QColor(arr[0].toInt(), arr[1].toInt(), arr[2].toInt());
    else if (arr.size() == 4)
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
  changed();
}

#define SCORE_MAKE_PAIR_COLOR(Col) vec.push_back(qMakePair(Col.color(), QStringLiteral(#Col)));
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

  for (auto& c : m_defaultPalette)
    vec_color.push_back({c.second.color(), c.first});

  return vec_color;
}

static bool pulse(QBrush& ref, bool pulse, int& idx)
{
  bool invert = false;
  auto col = ref.color();
  auto alpha = col.alphaF();
  if (pulse)
  {
    alpha += 0.02;
    idx++;
    if (alpha >= 1 || idx > 24)
    {
      invert = true;
      alpha = 1;
      idx = 24;
    }
    col.setAlphaF(alpha);
  }
  else
  {
    alpha -= 0.02;
    idx--;
    if (alpha <= 0.5 || idx < 0)
    {
      invert = true;
      alpha = 0.5;
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
  if (invert)
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

BrushSet::BrushSet(const QBrush& b) noexcept : brush{b}
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
