// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioStyle.hpp"

#include <score/model/Skin.hpp>
// TODO namespace
ScenarioStyle::ScenarioStyle(const score::Skin& s) noexcept
    :

    IntervalBase{&s.Base1}
    , IntervalSelected{&s.Base2}
    , IntervalPlayFill{&s.Base3}
    , IntervalPlayDashFill{&s.Pulse1}
    , IntervalWaitingDashFill{&s.Pulse2}
    , IntervalLoop{&s.Warn1}
    , IntervalWarning{&s.Warn2}
    , IntervalInvalid{&s.Warn3}
    , IntervalMuted{&s.Tender2}
    , IntervalDefaultLabel{&s.Gray}
    , IntervalDefaultBackground{&s.Transparent1}
    ,

    RackSideBorder{&s.Base1}
    ,

    IntervalFullViewParentSelected{&s.Emphasis1}
    ,

    IntervalHeaderText{&s.Light}
    , IntervalHeaderBottomLine{&s.Transparent1}
    , IntervalHeaderRackHidden{&s.Transparent1}
    , IntervalHeaderSideBorder{&s.Base1}
    ,

    ProcessViewBorder{&s.Gray}
    ,

    SlotFocus{&s.Base2}
    , SlotOverlayBorder{&s.Dark}
    , SlotOverlay{&s.Transparent2}
    , SlotHandle{&s.Transparent3}
    ,

    TimenodeDefault{&s.Gray}
    , TimenodeSelected{&s.Base2}
    ,

    EventDefault{&s.Emphasis4}
    , EventWaiting{&s.HalfLight}
    , EventPending{&s.Warn1}
    , EventHappened{&s.Base3}
    , EventDisposed{&s.Warn3}
    , EventSelected{&s.Base2}
    ,

    ConditionDefault{&s.Smooth3}
    , ConditionWaiting{&s.Gray}
    , ConditionDisabled{&s.Base1}
    , ConditionFalse{&s.Smooth1}
    , ConditionTrue{&s.Smooth2}
    ,

    StateOutline{&s.Light}
    , StateSelected{&s.Base2}
    , StateDot{&s.Base1}
    ,

    Background{&s.Background1}
    , ProcessPanelBackground{&s.Transparent1}
    ,

    TimeRulerBackground{&s.Background1}
    , TimeRuler{&s.Base1}
    , LocalTimeRuler{&s.Gray}
    , CommentBlockPen{Qt::white, 1.}
    , SeparatorPen{Qt::white, 2.}
    , SeparatorBrush{Qt::white}

    , TransparentPen{Qt::transparent}
    , TransparentBrush{Qt::transparent}
    , NoPen{Qt::NoPen}
    , NoBrush{Qt::NoBrush}
{
  update(s);
  QObject::connect(&s, &score::Skin::changed, [&] { this->update(s); });
}

void ScenarioStyle::setIntervalWidth(double w)
{
  IntervalSolidPen.setWidthF(3 * w);
  IntervalDashPen.setWidthF(3 * w);
  IntervalPlayPen.setWidthF(3 * w);
  IntervalPlayDashPen.setWidthF(3 * w);
  IntervalWaitingDashPen.setWidthF(3 * w);
}

ScenarioStyle& ScenarioStyle::instance() noexcept
{
  static ScenarioStyle s(score::Skin::instance());
  return s;
}

ScenarioStyle::ScenarioStyle() noexcept
{
  update(score::Skin::instance());
}

void ScenarioStyle::update(const score::Skin& skin)
{
  IntervalSolidPen
      = QPen{QBrush{Qt::black}, 3, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin};
  IntervalDashPen = [] {
    QPen pen{QBrush{Qt::black}, 3, Qt::CustomDashLine, Qt::FlatCap,
             Qt::MiterJoin};

    pen.setDashPattern({2., 4.});
    return pen;
  }();
  IntervalRackPen
      = QPen{Qt::black, 2, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin};
  IntervalPlayPen = IntervalSolidPen;
  IntervalPlayDashPen = IntervalDashPen;
  IntervalWaitingDashPen = IntervalDashPen;
  IntervalHeaderSeparator = QPen{IntervalHeaderSideBorder.getBrush(), 2,
                                 Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin};
  FullViewIntervalHeaderSeparator
      = QPen{IntervalHeaderSideBorder.getBrush(), 2, Qt::DashLine, Qt::FlatCap,
             Qt::MiterJoin};

  IntervalBrace = QPen{IntervalBase.getBrush(), 2, Qt::SolidLine, Qt::FlatCap,
                       Qt::MiterJoin};
  IntervalBraceSelected = IntervalBrace;
  IntervalBraceSelected.setBrush(IntervalSelected.getBrush());
  IntervalBraceWarning = IntervalBrace;
  IntervalBraceWarning.setBrush(IntervalWarning.getBrush());
  IntervalBraceInvalid = IntervalBrace;
  IntervalBraceInvalid.setBrush(IntervalInvalid.getBrush());
  IntervalHeaderTextPen = QPen{IntervalHeaderText.getBrush().color()};

  // don't: IntervalSolidPen.setCosmetic(true);
  // IntervalDashPen.setCosmetic(true);
  IntervalRackPen.setCosmetic(true);
  // IntervalPlayPen.setCosmetic(true);
  // IntervalPlayDashPen.setCosmetic(true);
  // IntervalWaitingDashPen.setCosmetic(true);

  ConditionPen = QPen{Qt::black, 2};
  ConditionTrianglePen = QPen{Qt::black, 2};
  ConditionTrianglePen.setCosmetic(true);

  TimenodePen = QPen{Qt::black, 2, Qt::DotLine, Qt::SquareCap, Qt::MiterJoin};
  TimenodeBrush = QBrush{Qt::black};
  TimenodePen.setCosmetic(true);

  MinimapPen = QPen{QColor("#99aaaaaa"), 1, Qt::SolidLine};
  MinimapPen.setCosmetic(true);
  MinimapBrush = QBrush{QColor::fromRgbF(0.31, 0.35, 0.44, 0.5)};

  StateTemporalPointBrush = QBrush{Qt::black};
  StateTemporalPointPen.setCosmetic(true);
  StateBrush = QBrush{Qt::black};
  EventPen = QPen{Qt::black, 0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin};
  EventPen.setCosmetic(true);
  EventBrush = QBrush{Qt::black};

  TimeRulerLargePen = QPen{TimeRuler.getBrush(), 2, Qt::SolidLine};
  TimeRulerLargePen.setCosmetic(true);
  TimeRulerSmallPen = QPen{TimeRuler.getBrush(), 1, Qt::SolidLine};
  TimeRulerSmallPen.setCosmetic(true);

  SlotHandlePen.setWidth(0);
  SlotHandlePen.setBrush(ProcessViewBorder.getBrush());

  MiniScenarioPen.setCosmetic(true);

  Bold10Pt = skin.SansFont;
  Bold10Pt.setPointSize(10);
  Bold10Pt.setBold(true);

  Bold12Pt = Bold10Pt;
  Bold12Pt.setPointSize(12);

  Medium7Pt = skin.SansFont;
  Medium7Pt.setPointSize(7);
  // Medium7Pt.setStyleStrategy(QFont::NoAntialias);
  // Medium7Pt.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
  Medium7Pt.setKerning(false);

  Medium8Pt = skin.SansFont;
  Medium8Pt.setPointSize(8);
  // Medium8Pt.setStyleStrategy(QFont::PreferBitmap);
  Medium8Pt.setKerning(false);

  Medium10Pt = skin.SansFont;
  Medium10Pt.setPointSize(10);
  // Medium7Pt.setStyleStrategy(QFont::NoAntialias);
  // Medium7Pt.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
  Medium10Pt.setKerning(false);

  Medium12Pt = skin.SansFont;
  Medium12Pt.setPointSize(12);
  // Medium12Pt.setStyleStrategy(QFont::NoAntialias);
  Medium12Pt.setKerning(false);

  AudioCablePen = QPen{QBrush{QColor("#88996666")}, 3., Qt::SolidLine,
                       Qt::RoundCap, Qt::RoundJoin};
  DataCablePen = QPen{QBrush{QColor("#88669966")}, 3., Qt::SolidLine,
                      Qt::RoundCap, Qt::RoundJoin};
  MidiCablePen = QPen{QBrush{QColor("#889966dd")}, 3., Qt::SolidLine,
                      Qt::RoundCap, Qt::RoundJoin};

  SelectedAudioCablePen = QPen{QBrush{QColor("#CC996666").lighter()}, 3.,
                               Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin};
  SelectedDataCablePen = QPen{QBrush{QColor("#CC669966").lighter()}, 3.,
                              Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin};
  SelectedMidiCablePen = QPen{QBrush{QColor("#CC9966dd").lighter()}, 3.,
                              Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin};

  AudioPortPen = QPen{QBrush{QColor("#FFAAAA")}, 1.5};
  DataPortPen = QPen{QBrush{QColor("#AAFFAA")}, 1.5};
  MidiPortPen = QPen{QBrush{QColor("#AAAAFF")}, 1.5};
  AudioPortBrush = AudioPortPen.brush().color().darker();
  DataPortBrush = DataPortPen.brush().color().darker();
  MidiPortBrush = MidiPortPen.brush().color().darker();

  GrayTextPen.setBrush(TimenodeDefault.getBrush());
  GrayTextPen.setCosmetic(true);

  SlotHeaderPen = QPen{IntervalHeaderSideBorder.getBrush(), 1,
                                 Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin};
}
