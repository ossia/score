#include "DefaultHeaderDelegate.hpp"
#include <Dataflow/Commands/EditConnection.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Dataflow/UI/CableItem.hpp>
#include <Dataflow/UI/PortItem.hpp>
#include <Process/Process.hpp>
#include <Explorer/Widgets/AddressAccessorEditWidget.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <QFormLayout>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QPainter>
namespace Scenario
{
class PortWidget : public QWidget
{
  public:
    PortWidget(const score::DocumentContext& ctx, Process::Port& p):
      m_edit{ctx.plugin<Explorer::DeviceDocumentPlugin>().explorer(), this}
    {
      auto lay = new QFormLayout{this};
      lay->addRow(tr("Address"), &m_edit);
    }

  private:
    Explorer::AddressAccessorEditWidget m_edit;
};

class PortPanel final
    : public QObject
    , public QGraphicsItem
{
    QRectF m_rect;
    QRectF m_widgrect;
    PortWidget* m_pw{};
    QGraphicsProxyWidget* m_proxy{};
  public:
    PortPanel(const score::DocumentContext& ctx, Process::Port& p, QGraphicsItem* parent):
      QGraphicsItem{parent}
    {
      m_pw = new PortWidget{ctx, p};
      m_proxy = new QGraphicsProxyWidget{this};
      m_proxy->setWidget(m_pw);
      m_proxy->setPos(10, 10);
      connect(m_proxy, &QGraphicsProxyWidget::geometryChanged,
              this, &PortPanel::updateRect);
      updateRect();
    }

    void updateRect()
    {
      prepareGeometryChange();

      m_widgrect = m_proxy->subWidgetRect(m_pw);
      m_rect = m_widgrect.adjusted(0, 0, 30, 20);
    }

    QRectF boundingRect() const override
    {
      return m_rect;
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
    {
      painter->setRenderHint(QPainter::Antialiasing, true);
      painter->setPen(QColor("#1A2024"));
      painter->setBrush(QColor("#1A2024"));

      QPainterPath p;
      p.moveTo(12, 12);
      p.lineTo(0, 22);
      p.lineTo(12, 34);
      p.lineTo(12, 12);
      p.closeSubpath();

      painter->drawPath(p);
      painter->fillPath(p, painter->brush());

      painter->drawRoundedRect(m_rect.adjusted(10, 0, 0, 0), 10, 10);

      painter->setRenderHint(QPainter::Antialiasing, false);
    }
};



void DefaultHeaderDelegate::onCreateCable(Dataflow::PortItem* p1, Dataflow::PortItem* p2)
{
  auto& ctx = presenter.context().context;
  auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();
  CommandDispatcher<> disp{ctx.commandStack};
  Process::CableData cd;
  cd.type = Process::CableType::ImmediateStrict;

  if(p1->port().outlet)
  {
    cd.source = p1->port();
    cd.sink = p2->port();
  }
  else
  {
    cd.source = p2->port();
    cd.sink = p1->port();
  }

  disp.submitCommand<Dataflow::CreateCable>(
        plug,
        getStrongId(plug.cables),
        cd);
}

DefaultHeaderDelegate::DefaultHeaderDelegate(Process::LayerPresenter& p)
  : presenter{p}
{
  con(presenter.model(), &Process::ProcessModel::prettyNameChanged,
      this, &DefaultHeaderDelegate::updateName);
  updateName();
  m_textcache.setFont(ScenarioStyle::instance().Medium8Pt);
  m_textcache.setCacheEnabled(true);

  int x = 16;
  for(auto& port : p.model().inlets())
  {
    auto item = new Dataflow::PortItem{*port, this};
    item->setPos(x, 16);
    connect(item, &Dataflow::PortItem::showPanel,
            this, [&,&pt=*port,item] {
      auto panel = new PortPanel{p.context().context, pt, nullptr};
      scene()->addItem(panel);
      panel->setPos(item->mapToScene(item->pos()));
    });
    connect(item, &Dataflow::PortItem::createCable,
            this, &DefaultHeaderDelegate::onCreateCable);
    x += 10;
  }
  x = 16;
  for(auto& port : p.model().outlets())
  {
    auto item = new Dataflow::PortItem{*port, this};
    item->setPos(x, 25);
    connect(item, &Dataflow::PortItem::showPanel,
            this, [&,&pt=*port,item] {
      auto panel = new PortPanel{p.context().context, pt, nullptr};
      scene()->addItem(panel);
      panel->setPos(item->mapToScene(item->pos()));
    });
    x += 10;
  }
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

void DefaultHeaderDelegate::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(ScenarioStyle::instance().IntervalHeaderSeparator);
  m_textcache.draw(painter, QPointF{8.,-4.});

  painter->setPen(ScenarioStyle::instance().TimenodePen);
  painter->setFont(ScenarioStyle::instance().Medium8Pt);
  painter->drawText(QPointF{4, 19}, "►");
  painter->drawText(QPointF{4, 28}, "◄");

  painter->setRenderHint(QPainter::Antialiasing, false);
}

}
