#pragma once
#include <QBrush>
#include <QCursor>
#include <QFont>
#include <QObject>
#include <QPair>
#include <QPen>
#include <QVector>

#include <score_lib_base_export.h>

#include <verdigris>
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

class SCORE_LIB_BASE_EXPORT Skin : public QObject
{
  W_OBJECT(Skin)
public:
  static Skin& instance() noexcept;
  ~Skin() override;

  void load(const QJsonObject& style);

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

public:
  void changed() E_SIGNAL(SCORE_LIB_BASE_EXPORT, changed)

private:
  void timerEvent(QTimerEvent* event) override;
  Skin() noexcept;

  struct NoGUI { };
  explicit Skin(NoGUI);

  struct color_map;
  color_map* m_colorMap{};
  QVector<QPair<QString, Brush>> m_defaultPalette;

  bool m_pulseDirection{false};
};
}
