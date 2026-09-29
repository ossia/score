#pragma once
#include <score/model/Skin.hpp>

#include <QBrush>
#include <QGuiApplication>
#include <QPen>

#include <utility>

namespace Midi
{
struct MidiStyle
{
  static const MidiStyle& instance() noexcept
  {
    static MidiStyle s;
    return s;
  }

  MidiStyle()
  {
    reload();
    QObject::connect(
        &score::Skin::instance(), &score::Skin::changed, qApp, [this] { reload(); });
  }

  void reload()
  {
    auto& skin = score::Skin::instance();

    noteBaseBrush = skin.Base4.main.brush;
    noteSelectedBasePen = QPen{skin.Base2.main.brush, 2};
    noteBasePen = QPen{skin.Base4.darker300.brush, 1};

    // The rows are tints over whatever is behind the layer, not fills: the
    // interval's bar lines in musical mode are drawn behind its processes and
    // must show through. White keys are lighter and black keys darker than
    // the ground, as on a keyboard: light skins swap Light and Dark (Light is
    // the foreground), so the two are ordered by lightness.
    QColor lighter = skin.Light.main.brush.color();
    QColor darker = skin.Dark.main.brush.color();
    if(lighter.lightness() < darker.lightness())
      std::swap(lighter, darker);
    lighter.setAlpha(14);
    darker.setAlpha(64);
    whiteKeyBrush = QBrush{lighter};
    blackKeyBrush = QBrush{darker};
    darkerBrush = skin.LightGray.main.brush;

    // Row separations: faint, in the skin's foreground; stronger between B
    // and C so the octaves read at a glance.
    QColor fg = skin.Light.main.brush.color();
    fg.setAlpha(16);
    semitonePen = QPen{fg, 1};
    semitonePen.setCosmetic(true);
    fg.setAlpha(48);
    octavePen = QPen{fg, 1};
    octavePen.setCosmetic(true);

    selectionPen
        = QPen{skin.Transparent1.main.brush, 2, Qt::DashLine, Qt::SquareCap,
               Qt::BevelJoin};
    selectionPen.setCosmetic(true);

    // The velocity ramp: the note colour at a saturation that follows the
    // velocity, so a quiet note is washed out and a loud one is the full
    // accent.
    const QColor base = noteBaseBrush.color();
    const double hue = base.hslHueF();
    const double lightness = base.lightnessF();
    for(std::size_t i = 0; i < std::size(paintedNoteBrush); i++)
    {
      QColor c = base;
      c.setHslF(hue, 0.2 + 1.5 * i / 256., lightness);
      paintedNoteBrush[i].setColor(c);
      paintedNoteBrush[i].setStyle(Qt::SolidPattern);
    }
  }

  QBrush whiteKeyBrush;
  QBrush blackKeyBrush;
  QBrush darkerBrush;
  const QBrush transparentBrush{Qt::transparent};
  QPen semitonePen;
  QPen octavePen;
  QPen selectionPen;

  QBrush noteBaseBrush;
  QPen noteSelectedBasePen;
  QPen noteBasePen;

  QBrush paintedNoteBrush[128];
};
}
