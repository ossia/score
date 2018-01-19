#include "PortItem.hpp"
#include <Dataflow/UI/CableItem.hpp>
#include <Process/Dataflow/Port.hpp>
#include <QDrag>
#include <QGraphicsSceneMoveEvent>
#include <QMimeData>
#include <QPainter>
#include <QCursor>
#include <QFormLayout>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QPainter>
#include <QDialog>
#include <score/tools/IdentifierGeneration.hpp>
#include <QDialogButtonBox>
#include <score/widgets/SignalUtils.hpp>
#include <QCheckBox>
#include <QMenu>
#include <QApplication>
#include <Process/Style/ScenarioStyle.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <State/MessageListSerialization.hpp>
#include <Dataflow/Commands/CreateModulation.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Dataflow/Commands/EditPort.hpp>
#include <Explorer/Widgets/AddressAccessorEditWidget.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/document/DocumentContext.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Automation/AutomationModel.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/editor/state/destination_qualifiers.hpp>
namespace Dataflow
{

void onCreateCable(const score::DocumentContext& ctx, Dataflow::PortItem* p1, Dataflow::PortItem* p2);

PortItem::port_map PortItem::g_ports;
PortItem* PortItem::clickedPort;
PortItem::PortItem(Process::Port& p, QGraphicsItem* parent)
  : QGraphicsItem{parent}
  , m_port{p}
{
  this->setCursor(QCursor());
  this->setAcceptDrops(true);
  this->setAcceptHoverEvents(true);
  this->setFlag(QGraphicsItem::ItemSendsScenePositionChanges, true);
  this->setToolTip(p.customData());

  g_ports.insert({&p, this});

  Path<Process::Port> path = p;
  for(auto c : CableItem::g_cables)
  {
    if(c.first->source().unsafePath() == path.unsafePath())
    {
      c.second->setSource(this);
      cables.push_back(c.second);
    }
    else if(c.first->sink().unsafePath() == path.unsafePath())
    {
      c.second->setTarget(this);
      cables.push_back(c.second);
    }
  }
}

PortItem::~PortItem()
{
  for(auto cable : cables)
  {
    if(cable->source() == this)
      cable->setSource(nullptr);
    if(cable->target() == this)
      cable->setTarget(nullptr);
  }
  auto it = g_ports.find(&m_port);
  if(it != g_ports.end())
    g_ports.erase(it);
}

void PortItem::setupMenu(QMenu&, const score::DocumentContext& ctx)
{

}

QRectF PortItem::boundingRect() const
{
  return {-m_diam/2., -m_diam/2., m_diam, m_diam};
}

void PortItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, true);

  auto& style = ScenarioStyle::instance();
  switch(m_port.type)
  {
    case Process::PortType::Audio:
      painter->setPen(style.AudioPortPen);
      painter->setBrush(style.AudioPortBrush);
      break;
    case Process::PortType::Message:
      painter->setPen(style.DataPortPen);
      painter->setBrush(style.DataPortBrush);
      break;
    case Process::PortType::Midi:
      painter->setPen(style.MidiPortPen);
      painter->setBrush(style.MidiPortBrush);
      break;
  }

  painter->drawEllipse(boundingRect());
  painter->setRenderHint(QPainter::Antialiasing, false);
}

void PortItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if(this->contains(event->pos()))
  {
    switch(event->button())
    {
      case Qt::RightButton:
        emit contextMenuRequested(event->scenePos(), event->screenPos());
        break;
      default:
        break;
    }
  }
  event->accept();
}

void PortItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if(QLineF(pos(), event->pos()).length() > QApplication::startDragDistance())
  {
    QDrag* d{new QDrag{this}};
    QMimeData* m = new QMimeData;
    clickedPort = this;
    m->setData(score::mime::port(), {});
    d->setMimeData(m);
    d->exec();
    connect(d, &QDrag::destroyed, this, [] {
      clickedPort = nullptr;
    });
  }
}

void PortItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(this->contains(event->pos()))
  {
    event->accept();
    switch(event->button())
    {
      case Qt::LeftButton:
        emit showPanel();
        break;
      default:
        break;
    }
  }
}

void PortItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  prepareGeometryChange();
  m_diam = 8.;
  update();
  event->accept();
}

void PortItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
  event->accept();
}

void PortItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  prepareGeometryChange();
  m_diam = 6.;
  update();
  event->accept();
}

void PortItem::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  prepareGeometryChange();
  m_diam = 8.;
  update();
  event->accept();
}

void PortItem::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void PortItem::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  prepareGeometryChange();
  m_diam = 6.;
  update();
  event->accept();
}

void PortItem::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  prepareGeometryChange();
  m_diam = 6.;
  update();
  auto& mime = *event->mimeData();

  auto& ctx = score::IDocument::documentContext(m_port);
  if(mime.formats().contains(score::mime::port()))
  {
    if(clickedPort && this != clickedPort)
    {
      onCreateCable(ctx, clickedPort, this);
    }
  }
  clickedPort = nullptr;

  CommandDispatcher<> disp{ctx.commandStack};
  if (mime.formats().contains(score::mime::addressettings()))
  {
    Mime<Device::FullAddressSettings>::Deserializer des{mime};
    Device::FullAddressSettings as = des.deserialize();

    if (as.address.path.isEmpty())
      return;

    disp.submitCommand(new ChangePortAddress{m_port, State::AddressAccessor{as.address}});
  }
  else if (mime.formats().contains(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    State::MessageList ml = des.deserialize();
    if (ml.empty())
      return;
    auto& newAddr = ml[0].address;

    if (newAddr == m_port.address())
      return;

    if (newAddr.address.path.isEmpty())
      return;

    disp.submitCommand(new ChangePortAddress{m_port, std::move(newAddr)});
  }
  event->accept();
}

QVariant PortItem::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
  switch(change)
  {
    case QGraphicsItem::ItemScenePositionHasChanged:
      for(auto cbl : cables)
      {
        cbl->resize();
      }
      break;
    case QGraphicsItem::ItemVisibleHasChanged:
    case QGraphicsItem::ItemSceneHasChanged:
      for(auto cbl : cables)
      {
        cbl->check();
      }
      break;
    default:
      break;
  }

  return QGraphicsItem::itemChange(change, value);
}







/** TODO
class PortPanel final
    : public QObject
    , public QGraphicsItem
{
    QRectF m_rect;
    QRectF m_widgrect;
    PortTooltip* m_pw{};
    QGraphicsProxyWidget* m_proxy{};
  public:
    PortPanel(const score::DocumentContext& ctx, Process::Port& p, QGraphicsItem* parent):
      QGraphicsItem{parent}
    {
      m_pw = new PortTooltip{ctx, p};
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
*/

template<typename Vec>
bool intersection_empty(const Vec& v1, const Vec& v2)
{
  for(const auto& e1 : v1)
  {
    for(const auto& e2 : v2)
    {
      if(e1 == e2)
        return false;
    }
  }
  return true;
}

class PortTooltip : public QWidget
{
  public:
    PortTooltip(const score::DocumentContext& ctx, Process::Port& p)
      : m_disp{ctx.commandStack}
      , m_edit{ctx.plugin<Explorer::DeviceDocumentPlugin>().explorer(), this}
    {
      auto lay = new QFormLayout{this};
      lay->addRow(p.customData(), (QWidget*)nullptr);
      lay->addRow(tr("Address"), &m_edit);
      if(auto outlet = qobject_cast<Process::Outlet*>(&p))
      {
        if(p.type == Process::PortType::Audio)
        {
          auto cb = new QCheckBox{this};
          cb->setChecked(outlet->propagate());
          lay->addRow(tr("Propagate"), cb);
          connect(cb, &QCheckBox::toggled,
                  this, [&ctx,&out=*outlet] (auto ok) {
            if(ok != out.propagate()) {
              CommandDispatcher<> d{ctx.commandStack};
              d.submitCommand<Dataflow::SetPortPropagate>(out, ok);
            }
          });
          con(*outlet, &Process::Outlet::propagateChanged,
              this, [=] (bool p) {
            if(p != cb->isChecked()) {
              cb->setChecked(p);
            }
          });
        }
      }

      m_edit.setAddress(p.address());
      con(p, &Process::Port::addressChanged,
          this, [this] (const State::AddressAccessor& addr) {
        if(addr != m_edit.address().address)
        {
          m_edit.setAddress(addr);
        }
      });
      con(m_edit, &Explorer::AddressAccessorEditWidget::addressChanged,
          this, [this,&p] (const Device::FullAddressAccessorSettings& set) {
        if(set.address != p.address())
          m_disp.submitCommand<Dataflow::ChangePortAddress>(p, set.address);
      });
    }

  private:
    CommandDispatcher<> m_disp;
    Explorer::AddressAccessorEditWidget m_edit;
};
class PortDialog final
    : public QDialog
{
  public:
    PortDialog(const score::DocumentContext& ctx, Process::Port& p, QWidget* parent):
      QDialog{parent}
    , m_pw{ctx, p}
    , m_bb{QDialogButtonBox::Ok}
    {
      this->setLayout(&m_lay);
      m_lay.addWidget(&m_pw);
      m_lay.addWidget(&m_bb);
      connect(&m_bb, &QDialogButtonBox::accepted,
              this, [=] {
        close();
      });
    }

  private:
    PortTooltip m_pw;
    QDialogButtonBox m_bb;
    QHBoxLayout m_lay;
};


void onCreateCable(const score::DocumentContext& ctx, Dataflow::PortItem* p1, Dataflow::PortItem* p2)
{
  auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();
  CommandDispatcher<> disp{ctx.commandStack};
  Process::CableData cd;
  cd.type = Process::CableType::ImmediateStrict;

  auto& port1 = p1->port();
  auto& port2 = p2->port();
  if(port1.parent() == port2.parent())
    return;

  if(!intersection_empty(port1.cables(), port2.cables()))
     return;

  if(port1.type != port2.type)
    return;

  auto o1 = qobject_cast<Process::Outlet*>(&port1);
  auto i2 = qobject_cast<Process::Inlet*>(&port2);
  if(o1 && i2)
  {
    cd.source = port1;
    cd.sink = port2;
  }
  else
  {
    auto o2 = qobject_cast<Process::Outlet*>(&port2);
    auto i1 = qobject_cast<Process::Inlet*>(&port1);
    if(o2 && i1)
    {
      cd.source = port2;
      cd.sink = port1;
    }
    else
    {
      return;
    }
  }

  disp.submitCommand<Dataflow::CreateCable>(
        plug,
        getStrongId(plug.cables),
        cd);
}

SCORE_PLUGIN_SCENARIO_EXPORT
void setupSimpleInlet(
    PortItem* item,
    Process::Inlet& port,
    const score::DocumentContext& ctx,
    QGraphicsItem* parent,
    QObject* context)
{
  QObject::connect(item, &Dataflow::PortItem::showPanel,
          context, [&] {
    auto panel = new PortDialog{ctx, port, nullptr};
    panel->exec();
    panel->deleteLater();
  });

  if(port.type == Process::PortType::Message)
  {
    QObject::connect(item, &Dataflow::PortItem::contextMenuRequested,
                     context, [&,item] (QPointF sp, QPoint p) {
      auto menu = new QMenu{};
      auto act = menu->addAction(QObject::tr("Create automation"));
      QObject::connect(act, &QAction::triggered, item, [item,&ctx] {
        item->on_createAutomation(ctx);
      }, Qt::QueuedConnection);
      item->setupMenu(*menu, ctx);
      menu->exec(p);
      menu->deleteLater();
    });
  }
}

SCORE_PLUGIN_SCENARIO_EXPORT
PortItem* setupInlet(Process::Inlet& port, const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context)
{
  auto item = new Dataflow::PortItem{port, parent};

  setupSimpleInlet(item, port, ctx, parent, context);

  return item;
}

PortItem* setupOutlet(Process::Outlet& port, const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context)
{
  auto item = new Dataflow::PortItem{port, parent};
  QObject::connect(item, &Dataflow::PortItem::showPanel,
          context, [&] {
    auto panel = new PortDialog{ctx, port, nullptr};
    panel->exec();
    panel->deleteLater();
  });
  return item;
}


void PortItem::on_createAutomation(const score::DocumentContext& ctx)
{
  QObject* obj = &m_port;
  while(obj)
  {
    auto parent = obj->parent();
    if(auto cst = qobject_cast<Scenario::IntervalModel*>(parent))
    {
      RedoMacroCommandDispatcher<Dataflow::CreateModulation> macro{ctx.commandStack};
      on_createAutomation(*cst, [&] (auto cmd) { macro.submitCommand(cmd); }, ctx);
      macro.commit();
      return;
    }
    else
    {
      obj = parent;
    }
  }
}

bool PortItem::on_createAutomation(
    Scenario::IntervalModel& cst,
    std::function<void(score::Command*)> macro,
    const score::DocumentContext& ctx)
{
  if(m_port.type != Process::PortType::Message)
    return false;
  auto ctrl = qobject_cast<Process::ControlInlet*>(&m_port);
  if(!ctrl)
    return false;

  auto make_cmd = new Scenario::Command::AddOnlyProcessToInterval{
                  cst,
                  Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), QString{}};
  macro(make_cmd);

  auto lay_cmd = new Scenario::Command::AddLayerInNewSlot{cst, make_cmd->processId()};
  macro(lay_cmd);

  auto dom = ctrl->domain();
  auto min = dom.get().convert_min<float>();
  auto max = dom.get().convert_max<float>();

  State::Unit unit = ctrl->address().qualifiers.get().unit;
  auto& autom = safe_cast<Automation::ProcessModel&>(cst.processes.at(make_cmd->processId()));
  macro(new Automation::SetUnit{autom, unit});
  macro(new Automation::SetMin{autom, min});
  macro(new Automation::SetMax{autom, max});

  auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();
  Process::CableData cd;
  cd.type = Process::CableType::ImmediateStrict;
  cd.source = *autom.outlet;
  cd.sink = m_port;

  macro(new Dataflow::CreateCable{plug, getStrongId(plug.cables), std::move(cd)});
  return true;
}

}
