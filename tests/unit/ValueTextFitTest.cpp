// The value a control draws under or over itself fits its width: in a smaller
// font, and cut if it has to be, rather than overflowing on both sides, as a
// plug-in printing "0.1176470588" for a parameter would.

#include <score_test/App.hpp>

#include <score/graphics/WidgetPresentation.hpp>
#include <score/model/Skin.hpp>

#include <score/graphics/GraphicsSliderBaseImpl.hpp>
#include <score/graphics/widgets/Constants.hpp>

#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <catch2/catch_test_macros.hpp>

namespace
{
qreal drawnWidth(const score::FittedText& t)
{
  return QFontMetricsF{*t.font}.horizontalAdvance(t.text);
}

int pixelSize(const QFont& f)
{
  return f.pixelSize();
}

//! The font controls draw their values with
const QFont& valueFont()
{
  return score::Skin::instance().Medium8Pt;
}

//! A slider drawing the text it is given, as a plug-in's slider draws the
//! text the plug-in formats
struct TextSlider final : score::QGraphicsSliderBase<TextSlider>
{
  friend struct score::QGraphicsSliderBase<TextSlider>;
  friend struct score::DefaultGraphicsSliderImpl;
  double m_value{0.5};
  double m_execValue{};
  bool m_grab{};
  bool m_hasExec{};
  QString text;

  TextSlider()
      : QGraphicsSliderBase{nullptr}
  {
    setRect(score::defaultSliderSize);
  }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* w)
      override
  {
    score::DefaultGraphicsSliderImpl::paint(
        *this, score::Skin::instance(), text, painter, w, option);
  }
};

//! The slider painted in an image; the number of pixels of its value on the
//! edges of the box the value is drawn in. Text too long for it is clipped
//! there, on both sides when centred, or wraps onto a second line.
int valueOnEdges(TextSlider& slider, QImage& img)
{
  const QRectF br = slider.boundingRect();
  img = QImage{int(br.width()), int(br.height()), QImage::Format_ARGB32_Premultiplied};
  img.fill(Qt::transparent);
  {
    QPainter p{&img};
    QStyleOptionGraphicsItem opt;
    slider.paint(&p, &opt, nullptr);
  }
  // As DefaultGraphicsSliderImpl::paint lays it out: 2 pixels in from each
  // side, below the bar
  const int left = 2, right = int(br.width()) - 3;
  const int top = int(slider.sliderRect().height()) + 1;
  int edges = 0;
  for(int y = top; y < img.height(); y++)
    for(int x : {left, right})
      if(qAlpha(img.pixel(x, y)) != 0)
        edges++;
  return edges;
}
}

TEST_CASE("A value that fits is drawn as it is", "[widgets][value_text]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const QFont& base = valueFont();
    for(const QString text : {"0.5", "-12.5 dB", "Sine", "440 Hz"})
    {
      const auto t = score::fitValueText(base, text, 76.);
      CHECK(*t.font == base);
      CHECK(t.text == text);
    }
  });
}

TEST_CASE("A long value is drawn smaller, whole", "[widgets][value_text]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const QFont& base = valueFont();
    // What uhhyou's plug-ins print for some parameters, in a slider's width
    for(const QString text : {"0.1176470588", "411764705 Hz", "12345678.90"})
    {
      const qreal width = 0.85 * QFontMetricsF{base}.horizontalAdvance(text);
      const auto t = score::fitValueText(base, text, width);
      CHECK(pixelSize(*t.font) < pixelSize(base));
      CHECK(t.text == text);
      CHECK(drawnWidth(t) <= width);
    }
  });
}

TEST_CASE("A value too long for the smallest font keeps its start", "[widgets][value_text]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const QFont& base = valueFont();
    const QString text = "0.117647058823529411764705882352941";
    const qreal width = 44.;
    const auto t = score::fitValueText(base, text, width);
    CHECK(pixelSize(*t.font) == pixelSize(base) - 3);
    REQUIRE(!t.text.isEmpty());
    CHECK(text.startsWith(t.text));
    CHECK(t.text.size() < text.size());
    CHECK(drawnWidth(t) <= width);
  });
}

TEST_CASE("Fonts get smaller one pixel at a time", "[widgets][value_text]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const QFont& base = valueFont();
    const QString text = "0.1176470588";
    int last = pixelSize(base) + 1;
    // Narrower and narrower: the font only gets smaller, never by a jump
    for(qreal width = 120.; width > 10.; width -= 2.)
    {
      const auto t = score::fitValueText(base, text, width);
      const int px = pixelSize(*t.font);
      CHECK(px <= last);
      CHECK(px >= pixelSize(base) - 3);
      last = px;
    }
    CHECK(last == pixelSize(base) - 3);
  });
}

TEST_CASE("A slider draws a plug-in's long value inside itself", "[widgets][value_text]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QImage strip;
    int row = 0;
    const QStringList texts{
        "0.5", "0.1176470588", "411764705882 Hz", "0.117647058823529411764705"};
    for(const auto& text : texts)
    {
      INFO(text.toStdString());
      TextSlider slider;
      slider.text = text;
      QImage img;
      CHECK(valueOnEdges(slider, img) == 0);

      // One image of them all, to look at:
      // SCORE_TEST_SHOT=slider-values.png test_unit_value_text_fit
      if(strip.isNull())
      {
        strip = QImage{img.width(), img.height() * int(texts.size()), img.format()};
        strip.fill(QColor{0x1e, 0x1e, 0x1e});
      }
      QPainter{&strip}.drawImage(0, img.height() * row++, img);
    }
    if(const auto path = qEnvironmentVariable("SCORE_TEST_SHOT"); !path.isEmpty())
      strip.scaled(strip.size() * 3).save(path);
  });
}
