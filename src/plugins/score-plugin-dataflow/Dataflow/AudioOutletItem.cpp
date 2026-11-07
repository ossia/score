#include "AudioOutletItem.hpp"

#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/PortAddressComboBox.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/Style/Pixmaps.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Inspector/InspectorLayout.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/graphics/ArrowDialog.hpp>
#include <score/graphics/widgets/QGraphicsCheckBox.hpp>
#include <score/graphics/widgets/QGraphicsSlider.hpp>
#include <score/model/Skin.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SetIcons.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QCheckBox>
#include <QSignalBlocker>
#include <QTimer>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QSpinBox>
#include <QToolButton>

namespace Dataflow
{
/**
 * @brief The panel shown when clicking an audio output in the nodal view.
 *
 * The gain and pan sub-ports (to plug cables or automations into), the gain
 * trim, propagation to the parent interval, the address and where the cables
 * go. Every edit goes through a command.
 */
class AudioOutletMiniPanel final : public score::ArrowDialog
{
  static constexpr double width = 160.;
  static constexpr double margin = 5.;
  static constexpr double lineHeight = 12.;
  static constexpr double titleY = 12.;
  static constexpr double gainY = 26.;
  static constexpr double panY = 44.;
  static constexpr double propagateY = 48.;
  static constexpr double addressY = 80.;
  static constexpr int maxCables = 3;

public:
  AudioOutletMiniPanel(
      const Process::AudioOutlet& port, const Process::Context& ctx,
      QGraphicsScene* parent)
      : ArrowDialog{parent}
      , m_port{port}
      , m_ctx{ctx}
  {
    auto gainPort = new AutomatablePortItem{*port.gainInlet, ctx, this};
    gainPort->setPos({3, gainY - 10});
    auto panPort = new AutomatablePortItem{*port.panInlet, ctx, this};
    panPort->setPos({3, panY - 10});

    m_gain = new score::QGraphicsSlider{this};
    m_gain->setRect(QRectF{0, 0, width - 52 - margin, 23});
    m_gain->setRange(0., 1., 1.);
    m_gain->setValue(port.gain());
    m_gain->setPos({52, gainY - 11});
    QObject::connect(m_gain, &score::QGraphicsSlider::sliderMoved, this, [this] {
      m_ctx.dispatcher.submit<Process::SetGain>(m_port, m_gain->value());
    });
    QObject::connect(m_gain, &score::QGraphicsSlider::sliderReleased, this, [this] {
      m_ctx.dispatcher.commit();
    });
    QObject::connect(&port, &Process::AudioOutlet::gainChanged, this, [this](double g) {
      if(!m_gain->moving)
        m_gain->setValue(g);
    });

    m_propagate = new score::QGraphicsCheckBox{this};
    m_propagate->setState(port.propagate());
    // Inside the panel: a click on an item whose origin is outside closes it
    // (ProcessGraphicsView::checkAndRemoveCurrentDialog)
    m_propagate->setPos({1, propagateY});
    m_propagate->setToolTip(QObject::tr("Send this output to the parent interval"));
    QObject::connect(m_propagate, &score::QGraphicsCheckBox::toggled, this, [this](bool b) {
      if(b != m_port.propagate())
        CommandDispatcher<>{m_ctx.commandStack}.submit<Process::SetPropagate>(m_port, b);
    });
    QObject::connect(
        &port, &Process::AudioOutlet::propagateChanged, this, [this](bool b) {
      m_propagate->setState(b);
      scheduleRefresh();
    });
    QObject::connect(
        &port, &Process::Port::addressChanged, this, [this] { scheduleRefresh(); });
    QObject::connect(
        &port, &Process::Port::cablesChanged, this, [this] { scheduleRefresh(); });

    refresh();
  }

  QRectF boundingRect() const override { return {0, 0, width, m_height}; }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override
  {
    auto& skin = score::Skin::instance();
    ArrowDialog::paint(painter, option, widget);

    painter->setFont(skin.SansFontSmall);
    painter->setPen(skin.Base4.lighter.pen1);
    painter->drawText(QPointF{15, gainY}, QObject::tr("Gain"));
    painter->drawText(QPointF{15, panY}, QObject::tr("Pan"));
    painter->drawText(QPointF{20, propagateY + 14}, QObject::tr("Propagate to parent"));

    const QRectF text{margin, 0, width - 2 * margin, lineHeight};
    auto elided = [&](const QString& s, double y) {
      painter->drawText(
          text.translated(0, y - lineHeight + 3),
          painter->fontMetrics().elidedText(s, Qt::ElideMiddle, int(text.width())),
          QTextOption{Qt::AlignLeft | Qt::AlignVCenter});
    };

    double y = addressY;
    painter->setPen(
        m_addressUnused ? skin.Warn1.main.pen1 : skin.Base4.lighter.pen1);
    elided(m_address, y);
    if(!m_addressNote.isEmpty())
    {
      y += lineHeight;
      elided(m_addressNote, y);
    }
    painter->setPen(skin.Base4.lighter.pen1);
    for(const auto& line : m_cables)
    {
      y += lineHeight;
      elided(line, y);
    }

    QFont bold = skin.SansFontSmall;
    bold.setBold(true);
    painter->setFont(bold);
    painter->setPen(skin.Base4.lighter.pen1);
    elided(Process::displayName(m_port), titleY);
  }

private:
  void scheduleRefresh()
  {
    if(m_pending)
      return;
    m_pending = true;
    QTimer::singleShot(0, this, [this] {
      m_pending = false;
      refresh();
    });
  }

  void refresh()
  {
    const auto& addr = m_port.address();
    const auto status = Process::AddressStatus::of(m_port, m_ctx);
    m_addressUnused = status.kind != Process::AddressStatus::Used;
    m_address = addr.isSet() ? addr.toString_unsafe() : QObject::tr("No address");
    // On a line of its own: the panel is too narrow for it next to the address
    if(!addr.isSet() || status.kind == Process::AddressStatus::Used)
      m_addressNote.clear();
    else if(status.kind == Process::AddressStatus::Unused)
      m_addressNote = QObject::tr("Not used: the cables take over");
    else
      m_addressNote = QObject::tr("Used while the cables are idle");
    setToolTip(status.reason);

    m_cables.clear();
    int count = 0;
    for(const auto& path : m_port.cables())
    {
      auto cable = path.try_find(m_ctx);
      if(!cable)
        continue;
      auto sink = cable->sink().try_find(m_ctx);
      if(!sink)
        continue;
      count++;
      if(count > maxCables)
        continue;
      if(auto proc = Process::parentProcess(sink))
        m_cables.push_back(
            QObject::tr("To %1: %2").arg(proc->metadata().getName(), sink->name()));
      else
        m_cables.push_back(QObject::tr("To %1").arg(Process::displayName(*sink)));
    }
    if(count > maxCables)
      m_cables.push_back(QObject::tr("and %n more", "", count - maxCables));

    const double h = addressY
                     + (m_cables.size() + (m_addressNote.isEmpty() ? 0 : 1)) * lineHeight
                     + 5;
    if(h != m_height)
    {
      prepareGeometryChange();
      m_height = h;
      positionChanged();
    }
    update();
  }

  //! The panel points at the port from above: it moves when it grows. The
  //! panel is a scene item, unscaled: the gap is in scene units, not in the
  //! port's, which shrink when the nodal view zooms out.
  void positionChanged()
  {
    if(auto item = m_anchor)
      setPos(item->scenePos() - QPointF{0., m_height + 7.});
  }

public:
  void setAnchor(QGraphicsItem* item)
  {
    m_anchor = item;
    positionChanged();
  }

private:
  const Process::AudioOutlet& m_port;
  const Process::Context& m_ctx;
  score::QGraphicsSlider* m_gain{};
  score::QGraphicsCheckBox* m_propagate{};
  QGraphicsItem* m_anchor{};
  QString m_address;
  QString m_addressNote;
  std::vector<QString> m_cables;
  double m_height{addressY + 5};
  bool m_addressUnused{};
  bool m_pending{};
};

void AudioOutletFactory::setupOutletInspector(
    const Process::Outlet& port, const score::DocumentContext& ctx, QWidget* parent,
    Inspector::Layout& lay, QObject* context)
{
  auto& outlet = static_cast<const Process::AudioOutlet&>(port);

  // Address and cables, then the two settings of the output side by side:
  // where the sound goes (Propagate) and how loud (Gain).
  Process::PortWidgetSetup::setupAddressPort(port, ctx, lay, parent);

  auto cb = new QCheckBox{QObject::tr("Propagate"), parent};
  cb->setObjectName(QStringLiteral("Propagate"));
  score::setHelp(cb, QObject::tr("Send this output to the parent interval"));
  cb->setChecked(outlet.propagate());
  lay.addRow("", cb);
  QObject::connect(cb, &QCheckBox::toggled, &outlet, [&ctx, &out = outlet](auto ok) {
    if(ok != out.propagate())
    {
      CommandDispatcher<> d{ctx.commandStack};
      d.submit<Process::SetPropagate>(out, ok);
    }
  });
  QObject::connect(&outlet, &Process::AudioOutlet::propagateChanged, cb, [=](bool p) {
    if(p != cb->isChecked())
    {
      cb->setChecked(p);
    }
  });

  // Linear gain, shown with its value; a right click types it
  auto gain = new score::ValueDoubleSlider{Qt::Horizontal, parent};
  gain->setObjectName(QStringLiteral("Gain"));
  gain->setRange(0., 1., 1.);
  gain->setValue(outlet.gain());
  QObject::connect(gain, &score::DoubleSlider::sliderMoved, gain, [&ctx, &outlet](double v) {
    ctx.dispatcher.submit<Process::SetGain>(outlet, v);
  });
  QObject::connect(
      gain, &score::DoubleSlider::sliderReleased, gain, [&ctx] { ctx.dispatcher.commit(); });
  QObject::connect(&outlet, &Process::AudioOutlet::gainChanged, gain, [gain](double g) {
    const QSignalBlocker block{gain};
    gain->setValue(g);
  });
  lay.addRow(QObject::tr("Gain"), gain);
}

AudioOutletItem::AudioOutletItem(
    Process::Port& p, const Process::Context& ctx, QGraphicsItem* parent)
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
    if(!m_subView)
    {
      auto panel = new AudioOutletMiniPanel{
          safe_cast<const Process::AudioOutlet&>(m_port), m_context, this->scene()};
      panel->setAnchor(this);
      m_subView = panel;
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

QVariant AudioOutletItem::itemChange(
    QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
  switch(change)
  {
    case QGraphicsItem::ItemScenePositionHasChanged:
      if(auto panel = static_cast<AudioOutletMiniPanel*>(m_subView.data()))
      {
        panel->setAnchor(this);
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
      const Process::MinMaxFloatOutlet& port, const Process::Context& ctx,
      QGraphicsScene* parent)
      : ArrowDialog{parent}
  {
    auto minPort = new AutomatablePortItem{*port.minInlet, ctx, this};
    minPort->setPos({3, 5});
    auto maxPort = new AutomatablePortItem{*port.maxInlet, ctx, this};
    maxPort->setPos({3, 18});
  }

  QRectF boundingRect() const override { return {0, 0, 50, 35}; }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override
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
    const Process::Outlet& port, const score::DocumentContext& ctx, QWidget* parent,
    Inspector::Layout& lay, QObject* context)
{
  Process::PortWidgetSetup::setupAlone(port, ctx, lay, parent);
}

MinMaxFloatOutletItem::MinMaxFloatOutletItem(
    Process::Port& p, const Process::Context& ctx, QGraphicsItem* parent)
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
    if(!m_subView)
    {
      m_subView = new MinMaxFloatOutletMiniPanel{
          safe_cast<const Process::MinMaxFloatOutlet&>(m_port), m_context,
          this->scene()};
      m_subView->setPos(scenePos() - QPointF{0., 42.});
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

QVariant MinMaxFloatOutletItem::itemChange(
    QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
  switch(change)
  {
    case QGraphicsItem::ItemScenePositionHasChanged:
      if(m_subView)
      {
        m_subView->setPos(scenePos() - QPointF{0., 42.});
      }
    default:
      break;
  }

  return AutomatablePortItem::itemChange(change, value);
}

}
