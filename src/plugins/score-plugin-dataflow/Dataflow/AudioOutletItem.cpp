#include "AudioOutletItem.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Inspector/InspectorLayout.hpp>
#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/AudioPortComboBox.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Style/Pixmaps.hpp>

#include <score/graphics/ArrowDialog.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/TextLabel.hpp>
#include <score/widgets/SetIcons.hpp>

#include <QCheckBox>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QSpinBox>
#include <QToolButton>

namespace Dataflow
{
class AudioOutletMiniPanel : public score::ArrowDialog
{
public:
  AudioOutletMiniPanel(
      const Process::AudioOutlet& port,
      const Process::Context& ctx,
      QGraphicsScene* parent)
      : ArrowDialog{parent}
  {
    auto gainPort = new AutomatablePortItem{*port.gainInlet, ctx, this};
    gainPort->setPos({3, 5});
    auto panPort = new AutomatablePortItem{*port.panInlet, ctx, this};
    panPort->setPos({3, 18});
  }

  QRectF boundingRect() const override { return {0, 0, 50, 35}; }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
  {
    auto& skin = score::Skin::instance();
    ArrowDialog::paint(painter, option, widget);

    painter->setFont(skin.SansFontSmall);
    painter->setPen(skin.Base4.lighter.pen1);
    painter->drawText(QPointF{15, 15}, QObject::tr("Gain"));
    painter->drawText(QPointF{15, 28}, QObject::tr("Pan"));
  }
};

void AudioOutletFactory::setupOutletInspector(
    const Process::Outlet& port,
    const score::DocumentContext& ctx,
    QWidget* parent,
    Inspector::Layout& lay,
    QObject* context)
{
  auto& outlet = static_cast<const Process::AudioOutlet&>(port);

  auto root = State::Address{"audio", {"out"}};
  auto& device = *ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  auto d = device.list().audioDevice();
  const auto& node = d->getNode(root);

  auto edit = Process::makeAddressCombo(root, node, port, ctx, parent);
  lay.addRow(edit);

  auto cb = new QCheckBox{QObject::tr("Propagate"), parent};
  cb->setChecked(outlet.propagate());
  lay.addRow(cb);
  QObject::connect(cb, &QCheckBox::toggled, &outlet, [&ctx, &out = outlet](auto ok) {
    if (ok != out.propagate())
    {
      CommandDispatcher<> d{ctx.commandStack};
      d.submit<Process::SetPropagate>(out, ok);
    }
  });
  QObject::connect(&outlet, &Process::AudioOutlet::propagateChanged, cb, [=](bool p) {
    if (p != cb->isChecked())
    {
      cb->setChecked(p);
    }
  });
}

AudioOutletItem::AudioOutletItem(
    Process::Port& p,
    const Process::Context& ctx,
    QGraphicsItem* parent)
    : AutomatablePortItem{p, ctx, parent}
{
}

AudioOutletItem::~AudioOutletItem()
{
  delete m_subView;
}

void AudioOutletItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  // if(event->pos().x() < 10)
  {
    if (!m_subView)
    {
      m_subView = new AudioOutletMiniPanel{
          safe_cast<const Process::AudioOutlet&>(m_port), m_context, this->scene()};
      this->scene()->addItem(m_subView);
      m_subView->setPos(this->mapToScene(0, -42));
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

QVariant
AudioOutletItem::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
  switch (change)
  {
    case QGraphicsItem::ItemScenePositionHasChanged:
      if (m_subView)
      {
        m_subView->setPos(this->mapToScene(0, -42));
      }
    default:
      break;
  }

  return AutomatablePortItem::itemChange(change, value);
}

class MinMaxFloatOutletMiniPanel : public score::ArrowDialog
{
public:
  MinMaxFloatOutletMiniPanel(
      const Process::MinMaxFloatOutlet& port,
      const Process::Context& ctx,
      QGraphicsScene* parent)
      : ArrowDialog{parent}
  {
    auto minPort = new AutomatablePortItem{*port.minInlet, ctx, this};
    minPort->setPos({3, 5});
    auto maxPort = new AutomatablePortItem{*port.maxInlet, ctx, this};
    maxPort->setPos({3, 18});
  }

  QRectF boundingRect() const override { return {0, 0, 50, 35}; }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
  {
    auto& skin = score::Skin::instance();
    ArrowDialog::paint(painter, option, widget);

    painter->setFont(skin.SansFontSmall);
    painter->setPen(skin.Base4.lighter.pen1);
    painter->drawText(QPointF{15, 15}, QObject::tr("Min"));
    painter->drawText(QPointF{15, 28}, QObject::tr("Max"));
  }
};

void MinMaxFloatOutletFactory::setupOutletInspector(
    const Process::Outlet& port,
    const score::DocumentContext& ctx,
    QWidget* parent,
    Inspector::Layout& lay,
    QObject* context)
{
  auto widg = new QWidget;
  auto hl = new score::MarginLess<QHBoxLayout>{widg};

  auto lab = new TextLabel{port.customData(), widg};
  hl->addWidget(lab);

  auto advBtn = new QToolButton{widg};
  advBtn->setIcon(makeIcon(QStringLiteral(":/icons/port_message.png")));
  hl->addWidget(advBtn);

  auto port_widg = Process::PortWidgetSetup::makeAddressWidget(port, ctx, parent);
  lay.addRow(widg, port_widg);
}

MinMaxFloatOutletItem::MinMaxFloatOutletItem(
    Process::Port& p,
    const Process::Context& ctx,
    QGraphicsItem* parent)
    : AutomatablePortItem{p, ctx, parent}
{
}

MinMaxFloatOutletItem::~MinMaxFloatOutletItem()
{
  delete m_subView;
}

void MinMaxFloatOutletItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  {
    if (!m_subView)
    {
      m_subView = new MinMaxFloatOutletMiniPanel{
          safe_cast<const Process::MinMaxFloatOutlet&>(m_port), m_context, this->scene()};
      scene()->addItem(m_subView);
      m_subView->setPos(this->mapToScene(0, -42));
    }
    else
    {
      delete m_subView;
      m_subView = nullptr;
    }
  }

  AutomatablePortItem::mousePressEvent(event);
}

void MinMaxFloatOutletItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  AutomatablePortItem::mouseMoveEvent(event);
}

void MinMaxFloatOutletItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  AutomatablePortItem::mouseReleaseEvent(event);
}

QVariant
MinMaxFloatOutletItem::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
  switch (change)
  {
    case QGraphicsItem::ItemScenePositionHasChanged:
      if (m_subView)
      {
        m_subView->setPos(this->mapToScene(0, -42));
      }
    default:
      break;
  }

  return AutomatablePortItem::itemChange(change, value);
}

}
