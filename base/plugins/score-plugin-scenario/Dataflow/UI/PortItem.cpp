#include "PortItem.hpp"

#include <ossia/editor/state/destination_qualifiers.hpp>
#include <ossia/network/domain/domain.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Dataflow/Commands/CreateModulation.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Dataflow/Commands/EditPort.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Widgets/AddressAccessorEditWidget.hpp>
#include <Process/Dataflow/CableItem.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <QApplication>
#include <QCheckBox>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDrag>
#include <QFormLayout>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSceneMoveEvent>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <State/MessageListSerialization.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/widgets/SignalUtils.hpp>
namespace Dataflow
{

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
    PortPanel(const score::DocumentContext& ctx, Process::Port& p,
QGraphicsItem* parent): QGraphicsItem{parent}
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

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
QWidget* widget) override
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

template <typename Vec>
bool intersection_empty(const Vec& v1, const Vec& v2)
{
  for (const auto& e1 : v1)
  {
    for (const auto& e2 : v2)
    {
      if (e1 == e2)
        return false;
    }
  }
  return true;
}

class PortDialog final : public QDialog
{
public:
  PortDialog(
      const score::DocumentContext& ctx, Process::Port& p, QWidget* parent)
      : QDialog{parent}, m_pw{ctx, p, parent}, m_bb{QDialogButtonBox::Ok}
  {
    this->setLayout(&m_lay);
    m_lay.addWidget(&m_pw);
    m_lay.addWidget(&m_bb);
    connect(&m_bb, &QDialogButtonBox::accepted, this, [=] { close(); });
  }

private:
  PortTooltip m_pw;
  QDialogButtonBox m_bb;
  QHBoxLayout m_lay;
};

void onCreateCable(
    const score::DocumentContext& ctx,
    Dataflow::PortItem* p1,
    Dataflow::PortItem* p2)
{
  auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();
  CommandDispatcher<> disp{ctx.commandStack};
  Process::CableData cd;
  cd.type = Process::CableType::ImmediateGlutton;

  auto& port1 = p1->port();
  auto& port2 = p2->port();
  if (port1.parent() == port2.parent())
    return;

  if (!intersection_empty(port1.cables(), port2.cables()))
    return;

  if (port1.type != port2.type)
    return;

  auto o1 = qobject_cast<Process::Outlet*>(&port1);
  auto i2 = qobject_cast<Process::Inlet*>(&port2);
  if (o1 && i2)
  {
    cd.source = port1;
    cd.sink = port2;
  }
  else
  {
    auto o2 = qobject_cast<Process::Outlet*>(&port2);
    auto i1 = qobject_cast<Process::Inlet*>(&port1);
    if (o2 && i1)
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
      plug, getStrongId(plug.cables), cd);
}

AutomatablePortItem::~AutomatablePortItem()
{
}

void AutomatablePortItem::setupMenu(
    QMenu& menu, const score::DocumentContext& ctx)
{
  auto act = menu.addAction(QObject::tr("Create automation"));
  QObject::connect(
      act, &QAction::triggered, this,
      [this, &ctx] { on_createAutomation(ctx); }, Qt::QueuedConnection);
}

void AutomatablePortItem::on_createAutomation(
    const score::DocumentContext& ctx)
{
  QObject* obj = &m_port;
  while (obj)
  {
    auto parent = obj->parent();
    if (auto cst = qobject_cast<Scenario::IntervalModel*>(parent))
    {
      RedoMacroCommandDispatcher<Dataflow::CreateModulation> macro{
          ctx.commandStack};
      on_createAutomation(
          *cst, [&](auto cmd) { macro.submitCommand(cmd); }, ctx);
      macro.commit();
      return;
    }
    else
    {
      obj = parent;
    }
  }
}

bool AutomatablePortItem::on_createAutomation(
    Scenario::IntervalModel& cst,
    std::function<void(score::Command*)> macro,
    const score::DocumentContext& ctx)
{
  if (m_port.type != Process::PortType::Message)
    return false;
  auto ctrl = qobject_cast<Process::ControlInlet*>(&m_port);
  if (!ctrl)
    return false;

  auto make_cmd = new Scenario::Command::AddOnlyProcessToInterval{
      cst, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(),
      QString{}};
  macro(make_cmd);

  auto lay_cmd
      = new Scenario::Command::AddLayerInNewSlot{cst, make_cmd->processId()};
  macro(lay_cmd);

  auto dom = ctrl->domain();
  auto min = dom.get().convert_min<float>();
  auto max = dom.get().convert_max<float>();

  State::Unit unit = ctrl->address().qualifiers.get().unit;
  auto& autom = safe_cast<Automation::ProcessModel&>(
      cst.processes.at(make_cmd->processId()));
  macro(new Automation::SetUnit{autom, unit});
  macro(new Automation::SetMin{autom, min});
  macro(new Automation::SetMax{autom, max});

  auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();
  Process::CableData cd;
  cd.type = Process::CableType::ImmediateGlutton;
  cd.source = *autom.outlet;
  cd.sink = m_port;

  macro(new Dataflow::CreateCable{plug, getStrongId(plug.cables),
                                  std::move(cd)});
  return true;
}

void AutomatablePortItem::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  prepareGeometryChange();
  m_diam = 6.;
  update();
  auto& mime = *event->mimeData();

  auto& ctx = score::IDocument::documentContext(m_port);
  if (mime.formats().contains(score::mime::port()))
  {
    if (clickedPort && this != clickedPort)
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

    disp.submitCommand(
        new ChangePortAddress{m_port, State::AddressAccessor{as.address}});
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

PortItem* AutomatablePortFactory::makeItem(
    Process::Inlet& port,
    const score::DocumentContext& ctx,
    QGraphicsItem* parent,
    QObject* context)
{
  auto item = new Dataflow::AutomatablePortItem{port, ctx, parent};
  /*
    QObject::connect(item, &Dataflow::PortItem::showPanel,
            context, [&] {
      auto panel = new PortDialog{ctx, port, nullptr};
      panel->exec();
      panel->deleteLater();
    });
  */
  return item;
}

PortItem* AutomatablePortFactory::makeItem(
    Process::Outlet& port,
    const score::DocumentContext& ctx,
    QGraphicsItem* parent,
    QObject* context)
{
  auto item = new Dataflow::AutomatablePortItem{port, ctx, parent};
  /*
    QObject::connect(item, &Dataflow::PortItem::showPanel,
                     context, [&] {
      auto panel = new PortDialog{ctx, port, nullptr};
      panel->exec();
      panel->deleteLater();
    });
  */
  return item;
}

PortTooltip::PortTooltip(
    const score::DocumentContext& ctx, const Process::Port& p, QWidget* parent)
    : QWidget{parent}
    , m_disp{ctx.commandStack}
    , m_edit{ctx.plugin<Explorer::DeviceDocumentPlugin>().explorer(), this}
{
  auto lay = new QFormLayout{this};
  lay->addRow(p.customData(), (QWidget*)nullptr);
  lay->addRow(tr("Address"), &m_edit);
  if (auto outlet = qobject_cast<const Process::Outlet*>(&p))
  {
    if (p.type == Process::PortType::Audio)
    {
      auto cb = new QCheckBox{this};
      cb->setChecked(outlet->propagate());
      lay->addRow(tr("Propagate"), cb);
      connect(cb, &QCheckBox::toggled, this, [&ctx, &out = *outlet](auto ok) {
        if (ok != out.propagate())
        {
          CommandDispatcher<> d{ctx.commandStack};
          d.submitCommand<Dataflow::SetPortPropagate>(out, ok);
        }
      });
      con(*outlet, &Process::Outlet::propagateChanged, this, [=](bool p) {
        if (p != cb->isChecked())
        {
          cb->setChecked(p);
        }
      });
    }
  }

  m_edit.setAddress(p.address());
  con(p, &Process::Port::addressChanged, this,
      [this](const State::AddressAccessor& addr) {
        if (addr != m_edit.address().address)
        {
          m_edit.setAddress(addr);
        }
      });
  con(m_edit, &Explorer::AddressAccessorEditWidget::addressChanged, this,
      [this, &p](const Device::FullAddressAccessorSettings& set) {
        if (set.address != p.address())
          m_disp.submitCommand<Dataflow::ChangePortAddress>(p, set.address);
      });
}
}
