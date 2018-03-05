#include "DefaultHeaderDelegate.hpp"
#include <Dataflow/Commands/EditConnection.hpp>
#include <Dataflow/Commands/EditPort.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Dataflow/UI/CableItem.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <Dataflow/UI/PortItem.hpp>
#include <Process/Process.hpp>
#include <Explorer/Widgets/AddressAccessorEditWidget.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Dataflow/Commands/EditPort.hpp>
#include <QFormLayout>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QPainter>
#include <QDialog>
#include <QDialogButtonBox>
#include <score/widgets/SignalUtils.hpp>
#include <QCheckBox>
#include <QMenu>
#include <QTextLayout>
namespace Scenario
{

static QGlyphRun makeGlyphs(const char* glyph)
{
  QTextLayout lay;
  lay.setFont(ScenarioStyle::instance().Medium8Pt);
  lay.setCacheEnabled(true);

  lay.setText(glyph);
  lay.beginLayout();
  QTextLine line = lay.createLine();
  lay.endLayout();
  line.setPosition(QPointF{0., 0.});

  auto r = line.glyphRuns();
  SCORE_ASSERT(r.size() >= 1);
  return r[0];
}

static const QGlyphRun& fromGlyph()
{
  static const QGlyphRun gl{makeGlyphs(">")};
  return gl;
}
static const QGlyphRun& toGlyph()
{
  static const QGlyphRun gl{makeGlyphs("<")};
  return gl;
}

DefaultHeaderDelegate::DefaultHeaderDelegate(Process::LayerPresenter& p)
  : presenter{p}
{
  con(presenter.model(), &Process::ProcessModel::prettyNameChanged,
      this, &DefaultHeaderDelegate::updateName);
  updateName();

  con(p.model(), &Process::ProcessModel::inletsChanged,
      this, [=] { updatePorts(); });
  con(p.model(), &Process::ProcessModel::inletsChanged,
      this, [=] { updatePorts(); });
  con(p.model(), &Process::ProcessModel::benchmark,
      this, [=] (double d) { updateBench(d); });
  con(p.model().selection, &Selectable::changed,
      this, [=] (bool b) {
    m_sel = b;
    update();
  });
  updatePorts();
}

DefaultHeaderDelegate::~DefaultHeaderDelegate()
{

}

void DefaultHeaderDelegate::updateBench(double d)
{
  QTextLayout lay;
  lay.setFont(ScenarioStyle::instance().Medium8Pt);
  lay.setCacheEnabled(false);

  lay.setText(QString::number(d, 'g', 3));
  lay.beginLayout();
  QTextLine line = lay.createLine();
  lay.endLayout();
  line.setPosition(QPointF{0., 0.});

  auto r = line.glyphRuns();

  if(r.size() > 0)
    m_bench = std::move(r[0]);
  else
    m_bench.clear();

  update();
}

void DefaultHeaderDelegate::updateName()
{
  QTextLayout lay;
  lay.setFont(ScenarioStyle::instance().Medium8Pt);
  lay.setCacheEnabled(true);

  lay.setText(presenter.model().prettyName());
  lay.beginLayout();
  QTextLine line = lay.createLine();
  lay.endLayout();
  line.setPosition(QPointF{0., 0.});

  auto r = line.glyphRuns();

  if(r.size() > 0)
    m_line = std::move(r[0]);
  else
    m_line.clear();

  update();
}

void DefaultHeaderDelegate::setSize(QSizeF sz)
{
  auto old_w = this->boundingRect().width();
  GraphicsShapeItem::setSize(sz);
  const auto pw = minPortWidth();
  if(sz.width() < pw || sz.width() < pw)
  {
    int i = 0;
    for(auto item : m_inPorts)
    {
      item->setPos(0., qreal(i) * sz.height() / qreal(m_inPorts.size()));
      i++;
    }

    i = 0;
    for(auto item : m_outPorts)
    {
      item->setPos(sz.width(), qreal(i) * sz.height() / qreal(m_outPorts.size()));
      i++;
    }
  }
  else if(old_w < pw && sz.width() > pw)
  {
    qreal x = 16;
    for(auto item : m_inPorts)
    {
      item->setPos(x, 15.);
      x += 10.;
    }

    x = 16.;
    for(auto item : m_outPorts)
    {
      item->setPos(x, 24.);
      x += 10.;
    }
  }
}

double DefaultHeaderDelegate::minPortWidth() const
{
  qreal inWidth =  10. * m_inPorts.size();
  qreal outWidth = 10. * m_outPorts.size();
  return std::max(inWidth, outWidth);
}

void DefaultHeaderDelegate::updatePorts()
{
  qDeleteAll(m_inPorts);
  m_inPorts.clear();
  qDeleteAll(m_outPorts);
  m_outPorts.clear();
  const auto& ctx = presenter.context().context;

  qreal x = 16;
  for(Process::Inlet* port : presenter.model().inlets())
  {
    if(port->hidden)
      continue;
    auto item = Dataflow::setupInlet(*port, ctx, this, this);
    item->setPos(x, 15.);
    m_inPorts.push_back(item);
    x += 10.;
  }

  x = 16.;
  for(Process::Outlet* port : presenter.model().outlets())
  {
    if(port->hidden)
      continue;
    auto item = Dataflow::setupOutlet(*port, ctx, this, this);
    item->setPos(x, 24.);
    m_outPorts.push_back(item);
    x += 10.;
  }
}

void DefaultHeaderDelegate::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const auto w = boundingRect().width();
  if(w > minPortWidth()) {
    const auto& style = ScenarioStyle::instance();

    painter->setRenderHint(QPainter::Antialiasing, true);

    painter->setPen(m_sel ? style.IntervalHeaderTextPen : style.GrayTextPen);
    painter->drawGlyphRun(QPointF{8.,1.}, m_line);
    painter->drawGlyphRun(QPointF{w - 32.,1.}, m_bench);
    painter->drawGlyphRun(QPointF{4., 10.}, fromGlyph());
    painter->drawGlyphRun(QPointF{4., 20.}, toGlyph());

    painter->setRenderHint(QPainter::Antialiasing, false);
  }
}

}
