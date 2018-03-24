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
#include <score/widgets/GraphicsItem.hpp>
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
#include <QApplication>
#include <QTextLayout>
namespace Scenario
{
static QImage makeGlyphs(const QString& glyph, const QPen& pen)
{
  QImage path;

  QTextLayout lay;
  lay.setFont(ScenarioStyle::instance().Medium8Pt);
  lay.setText(glyph);
  lay.beginLayout();
  QTextLine line = lay.createLine();
  lay.endLayout();
  line.setPosition(QPointF{0., 0.});

  auto r = line.glyphRuns();
  if(r.size() >= 1)
  {
    auto rect = line.naturalTextRect();
    double ratio = qApp->devicePixelRatio();
    path = QImage(rect.width() * ratio, rect.height() * ratio, QImage::Format_ARGB32_Premultiplied);
    path.setDevicePixelRatio(ratio);
    path.fill(Qt::transparent);

    QPainter p{&path};
    p.setPen(pen);
    p.drawGlyphRun(QPointF{0, 0}, r[0]);
  }

  return path;
}

static QImage& fromGlyphGray()
{
  static QImage gl{makeGlyphs(">", ScenarioStyle::instance().GrayTextPen)};
  return gl;
}
static QImage& toGlyphGray()
{
  static QImage gl{makeGlyphs("<", ScenarioStyle::instance().GrayTextPen)};
  return gl;
}
static QImage& fromGlyphWhite()
{
  static QImage gl{makeGlyphs(">", ScenarioStyle::instance().IntervalHeaderTextPen)};
  return gl;
}
static QImage& toGlyphWhite()
{
  static QImage gl{makeGlyphs("<", ScenarioStyle::instance().IntervalHeaderTextPen)};
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
    updateName();
    update();
  });
  updatePorts();
}

DefaultHeaderDelegate::~DefaultHeaderDelegate()
{

}

void DefaultHeaderDelegate::updateBench(double d)
{
  const auto& style = ScenarioStyle::instance();
  m_bench = makeGlyphs(QString::number(d, 'g', 3), m_sel ? style.IntervalHeaderTextPen : style.GrayTextPen);
  update();
}

void DefaultHeaderDelegate::updateName()
{
  const auto& style = ScenarioStyle::instance();
  m_line = makeGlyphs(presenter.model().prettyName(), m_sel ? style.IntervalHeaderTextPen : style.GrayTextPen);
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
    painter->drawImage(QPointF{8.,1.}, m_line);
    painter->drawImage(QPointF{w - 32.,1.}, m_bench);
    if(m_sel)
    {
      painter->drawImage(QPointF{4., 10.}, fromGlyphWhite());
      painter->drawImage(QPointF{4., 20.}, toGlyphWhite());
    }
    else
    {
      painter->drawImage(QPointF{4., 10.}, fromGlyphGray());
      painter->drawImage(QPointF{4., 20.}, toGlyphGray());
    }
  }
}

}
