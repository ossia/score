#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/HeaderDelegate.hpp>
#include <Process/Process.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/graphics/YPos.hpp>

#include <QApplication>
#include <QPainter>
#include <QTextLayout>
#include <QTextLine>

#include <Effect/EffectLayer.hpp>
namespace Process
{
QPixmap makeGlyphs(const QString& glyph, const QPen& pen)
{
  QImage path;

  QTextLayout lay;
  lay.setFont(score::Skin::instance().Medium8Pt);
  lay.setText(glyph);
  lay.beginLayout();
  QTextLine line = lay.createLine();
  lay.endLayout();
  line.setPosition(QPointF{0., 0.});

  auto r = line.glyphRuns();
  if (r.size() >= 1)
  {
    auto rect = line.naturalTextRect();
    double ratio = qApp->devicePixelRatio();
    path = QImage(
        rect.width() * ratio,
        rect.height() * ratio,
        QImage::Format_ARGB32_Premultiplied);
    path.setDevicePixelRatio(ratio);
    path.fill(Qt::transparent);

    QPainter p{&path};
    p.setPen(pen);
    p.drawGlyphRun(QPointF{0, 0}, r[0]);
  }

  return QPixmap::fromImage(path);
}

static double portY()
{
  //  static double& foo = [] () -> double& {
  //    static double d;
  //    auto s = new QSpinBox; s->setRange(0, 50);
  //    QObject::connect(s, SignalUtils::QSpinBox_valueChanged_int(),
  //            [&] (int x) { d = x; });
  //    s->show();
  //    return d;
  //  }();
  return SCORE_YPOS(7., 8.);
}

static double textY()
{
  return 1.;
}
static double minPortWidth()
{
  return 20.;
}

DefaultHeaderDelegate::DefaultHeaderDelegate(const Process::LayerPresenter& p)
    : HeaderDelegate{p}
{
  if (auto ui_btn = Process::makeExternalUIButton(
          p.model(), p.context().context, this, this))
    ui_btn->setPos({0, 2});

  con(p.model(), &Process::ProcessModel::prettyNameChanged, this, [=] {
    updateText();
    update();
  });

  con(p.model(), &Process::ProcessModel::inletsChanged, this, [=] {
    updatePorts();
  });
  con(p.model(), &Process::ProcessModel::outletsChanged, this, [=] {
    updatePorts();
  });
  con(p.model(), &Process::ProcessModel::benchmark, this, [=](double d) {
    updateBench(d);
  });
  con(p.model().selection,
      &Selectable::changed,
      this,
      [=](bool b) {
        m_sel = b;
        updateText();
        update();
      },
      Qt::QueuedConnection);

  updateText();
}

DefaultHeaderDelegate::~DefaultHeaderDelegate() {}

void DefaultHeaderDelegate::updateBench(double d)
{
  const auto& style = Process::Style::instance();
  m_bench = makeGlyphs(
      QString::number(d, 'g', 3),
      m_sel ? style.IntervalHeaderTextPen() : style.SlotHeaderTextPen());
  update();
}

void DefaultHeaderDelegate::updateText()
{
  if (presenter)
  {
    auto& style = Process::Style::instance();
    auto& model = presenter->model();
    const QPen& pen = m_sel ? style.IntervalHeaderTextPen() : textPen(style, model);
    m_line = makeGlyphs(model.prettyName(), pen);
    m_fromGlyph = makeGlyphs("I:", pen);
    m_toGlyph = makeGlyphs("O:", pen);
    update();
    updatePorts();
  }
}

const QPen& DefaultHeaderDelegate::textPen(Style& style, const Process::ProcessModel& model) const noexcept
{
  score::ModelMetadata* parent_col = model.parent()->template findChild<score::ModelMetadata*>({}, Qt::FindDirectChildrenOnly);
  auto& b = parent_col->getColor().getBrush();
  if(&b == &style.IntervalDefaultBackground())
    return style.skin.HalfLight.main.pen_cosmetic;
  else
    return b.lighter180.pen_cosmetic;
}

void DefaultHeaderDelegate::setSize(QSizeF sz)
{
  GraphicsShapeItem::setSize(sz);

  for (auto p : m_inPorts)
  {
    if (p->x() > sz.width())
    {
      if (p->isVisible())
        p->setPortVisible(false);
    }
    else
    {
      if (!p->isVisible())
        p->setPortVisible(true);
    }
  }

  for (auto p : m_outPorts)
  {
    if (p->x() > sz.width())
    {
      if (p->isVisible())
        p->setPortVisible(false);
    }
    else
    {
      if (!p->isVisible())
        p->setPortVisible(true);
    }
  }
}

void DefaultHeaderDelegate::updatePorts()
{
  if (!presenter)
    return;
  qDeleteAll(m_inPorts);
  m_inPorts.clear();
  qDeleteAll(m_outPorts);
  m_outPorts.clear();
  const auto& ctx = presenter->context().context;

  qreal x = 10.;
  const auto& inlets = presenter->model().inlets();

  auto& portFactory
      = score::AppContext().interfaces<Process::PortFactoryList>();
  if (!inlets.empty())
  {
    x += 16.;
    for (Process::Inlet* port : inlets)
    {
      if (port->hidden)
        continue;
      auto fact = portFactory.get(port->concreteKey());
      auto item = fact->makeItem(*port, ctx, this, this);
      item->setPos(x, portY());
      m_inPorts.push_back(item);
      x += 10.;
    }
  }

  x += 24. + m_line.width();
  for (Process::Outlet* port : presenter->model().outlets())
  {
    if (port->hidden)
      continue;
    auto fact = portFactory.get(port->concreteKey());
    auto item = fact->makeItem(*port, ctx, this, this);
    item->setPos(x, portY());
    m_outPorts.push_back(item);
    x += 10.;
  }
}

void DefaultHeaderDelegate::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  constexpr auto start = 10.;
  const auto w = boundingRect().width();
  if (w > minPortWidth())
  {
    const bool no_in = m_inPorts.empty();
    const bool no_out = m_outPorts.empty();
    if (no_in && no_out)
    {
      painter->drawPixmap(QPointF{start, SCORE_YPOS(1., -1.)}, m_line);
      painter->drawPixmap(QPointF{w - 32., SCORE_YPOS(1., -1.)}, m_bench);
    }
    else if(no_in)
    {
      painter->drawPixmap(QPointF{start, SCORE_YPOS(1., -1.)}, m_line);
      painter->drawPixmap(QPointF{w - 32., SCORE_YPOS(1., -1.)}, m_bench);
      painter->drawPixmap(
            QPointF{start + 8. + m_line.width(),
                    textY() + SCORE_YPOS(0., -2.)},
            m_toGlyph);
    }
    else if(no_out)
    {
      double startText = start + 16. + m_inPorts.size() * 10.;
      painter->drawPixmap(QPointF{startText, SCORE_YPOS(1., -1.)}, m_line);
      painter->drawPixmap(QPointF{w - 32., SCORE_YPOS(1., -1.)}, m_bench);
      painter->drawPixmap(
          QPointF{start + 4., textY() + SCORE_YPOS(0., -2.)},
          m_fromGlyph);
    }
    else
    {
      double startText = start + 16. + m_inPorts.size() * 10.;
      painter->drawPixmap(QPointF{startText, SCORE_YPOS(1., -1.)}, m_line);
      painter->drawPixmap(QPointF{w - 32., SCORE_YPOS(1., -1.)}, m_bench);
        painter->drawPixmap(
            QPointF{start + 4., textY() + SCORE_YPOS(0., -2.)},
            m_fromGlyph);
        painter->drawPixmap(
            QPointF{startText + 8. + m_line.width(),
                    textY() + SCORE_YPOS(0., -2.)},
            m_toGlyph);
    }
  }
}
}
