#include "AudioOutletItem.hpp"
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/Commands/EditPort.hpp>
#include <Process/Style/Pixmaps.hpp>
#include <Process/Dataflow/AudioPortComboBox.hpp>
#include <Inspector/InspectorLayout.hpp>
#include <QCheckBox>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QGraphicsScene>

namespace Dataflow
{
void AudioOutletFactory::setupOutletInspector(
        Process::Outlet &port,
        const score::DocumentContext &ctx,
        QWidget *parent,
        Inspector::Layout &lay,
        QObject *context)
{
    auto& outlet = static_cast<Process::AudioOutlet&>(port);

    auto root = State::Address{"audio", {"out"}};
    auto& device = *ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
    auto d = device.list().audioDevice();
    const auto& node = d->getNode(root);

    auto edit = Process::makeAddressCombo(root, node, port, ctx, parent);
    lay.addRow(edit);

    auto cb = new QCheckBox{parent};
    cb->setChecked(outlet.propagate());
    lay.addRow(QObject::tr("Propagate"), cb);
    QObject::connect(cb, &QCheckBox::toggled,
                     &outlet, [&ctx, &out = outlet](auto ok) {
          if (ok != out.propagate())
          {
            CommandDispatcher<> d{ctx.commandStack};
            d.submit<Process::SetPropagate>(out, ok);
          }
        });
    QObject::connect(&outlet, &Process::AudioOutlet::propagateChanged,
        cb, [=](bool p) {
      if (p != cb->isChecked())
      {
        cb->setChecked(p);
      }
    });
}

AudioOutletItem::AudioOutletItem(Process::Port& p, const Process::Context& ctx, QGraphicsItem* parent):
  AutomatablePortItem{p, ctx, parent}
{

}

AudioOutletItem::~AudioOutletItem()
{
  if(m_subView)
    delete m_subView;
}

QRectF AudioOutletItem::boundingRect() const
{
  return PortItem::boundingRect().adjusted(0,0,10,0);
}

void AudioOutletItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const auto& pix = Process::Pixmaps::instance();
  if(m_subView)
  {
    painter->drawPixmap(0, 0, pix.portHandleOpen);
  }
  else
  {
    painter->drawPixmap(0, 0, pix.portHandleClosed);
  }
  const QPixmap& img = portImage(m_port.type, m_inlet, m_diam == 8., m_highlight);
  painter->drawPixmap(10, 0, img);
}

class AudioOutletMiniPanel : public QGraphicsItem
{
public:
  AudioOutletMiniPanel(
      const Process::AudioOutlet& port,
      const Process::Context& ctx,
      QGraphicsItem* parent)
    : QGraphicsItem{parent}
  {
    auto gainPort = new AutomatablePortItem{port.gainInlet, ctx, this};
    gainPort->setPos(10, 10);
    auto panPort = new AutomatablePortItem{port.panInlet, ctx, this};
    panPort->setPos(10, 25);
  }

  QRectF boundingRect() const override
  {
    return {0, 0, 70, 40};
  }
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
  {
    auto& skin = score::Skin::instance();
    painter->setPen(skin.Base5.lighter.pen1);
    painter->setBrush(skin.Base5.darker.brush);
    painter->drawRoundedRect(boundingRect(), 3, 3);

    painter->setFont(skin.SansFontSmall);
    painter->setPen(skin.Base4.lighter.pen1);
    painter->drawText(QPointF{20, 20}, QObject::tr("Gain"));
    painter->drawText(QPointF{20, 40}, QObject::tr("Pan"));
  }
};

void AudioOutletItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if(event->pos().x() < 10)
  {
    if(!m_subView)
    {
      m_subView = new AudioOutletMiniPanel{safe_cast<const Process::AudioOutlet&>(m_port), m_context, nullptr};
      scene()->addItem(m_subView);
      m_subView->setPos(this->mapToScene(0, -50));

    }
    else
    {
      delete m_subView;
      m_subView = nullptr;
    }
  }

  AutomatablePortItem::mousePressEvent(event);
}

void AudioOutletItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  AutomatablePortItem::mouseMoveEvent(event);
}

void AudioOutletItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  AutomatablePortItem::mouseReleaseEvent(event);
}


QVariant AudioOutletItem::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
  switch (change)
  {
    case QGraphicsItem::ItemScenePositionHasChanged:
      if(m_subView)
      {
        m_subView->setPos(this->mapToScene(0, -50));
      }
    default:
      break;
  }

  return AutomatablePortItem::itemChange(change, value);
}

}
