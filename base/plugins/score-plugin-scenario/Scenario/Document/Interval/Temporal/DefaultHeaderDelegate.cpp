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
namespace Scenario
{


DefaultHeaderDelegate::DefaultHeaderDelegate(Process::LayerPresenter& p)
  : presenter{p}
{
  con(presenter.model(), &Process::ProcessModel::prettyNameChanged,
      this, &DefaultHeaderDelegate::updateName);
  updateName();
  m_textcache.setFont(ScenarioStyle::instance().Medium8Pt);
  m_textcache.setCacheEnabled(true);

  con(p.model(), &Process::ProcessModel::inletsChanged,
      this, [=] { updatePorts(); });
  con(p.model(), &Process::ProcessModel::inletsChanged,
      this, [=] { updatePorts(); });
  updatePorts();
}

void DefaultHeaderDelegate::updateName()
{
  m_textcache.setText(presenter.model().prettyName());
  m_textcache.beginLayout();

  QTextLine line = m_textcache.createLine();
  line.setPosition(QPointF{0., 0.});

  m_textcache.endLayout();

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
  for(auto& port : presenter.model().inlets())
  {
    if(port->hidden)
      continue;
    auto item = Dataflow::setupInlet(*port, ctx, this, this);
    item->setPos(x, 15.);
    m_inPorts.push_back(item);
    x += 10.;
  }

  x = 16.;
  for(auto& port : presenter.model().outlets())
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
  if(boundingRect().width() > minPortWidth()) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(ScenarioStyle::instance().IntervalHeaderSeparator);
    m_textcache.draw(painter, QPointF{8.,-3.});

    painter->setPen(ScenarioStyle::instance().TimenodePen);
    painter->setFont(ScenarioStyle::instance().Medium8Pt);
    painter->drawText(QPointF{4, 18}, "→");
    painter->drawText(QPointF{4, 27}, "←");

    painter->setRenderHint(QPainter::Antialiasing, false);
  }
}

}
