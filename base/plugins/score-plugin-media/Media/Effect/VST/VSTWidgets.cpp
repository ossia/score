#include "VSTWidgets.hpp"

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Dataflow/Commands/CreateModulation.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Dataflow/UI/PortItem.hpp>
#include <Engine/Node/CommonWidgets.hpp>
#include <Media/Commands/VSTCommands.hpp>
#include <Media/Effect/VST/VSTControl.hpp>
#include <QAction>
#include <QMenu>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <QGraphicsScene>
#include <wobjectimpl.h>
#include <Media/Effect/Settings/Model.hpp>
#include <score/widgets/Pixmap.hpp>

W_OBJECT_IMPL(Media::VST::VSTWindow)
W_OBJECT_IMPL(Media::VST::VSTGraphicsSlider)
namespace Media::VST
{

VSTGraphicsSlider::VSTGraphicsSlider(
    AEffect* fx, int num, QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  this->fx = fx;
  this->num = num;
  this->m_value = fx->getParameter(fx, num);
  this->setAcceptedMouseButtons(Qt::LeftButton);
}

void VSTGraphicsSlider::setRect(QRectF r)
{
  prepareGeometryChange();
  m_rect = r;
}

void VSTGraphicsSlider::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

double VSTGraphicsSlider::value() const
{
  return m_value;
}

void VSTGraphicsSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  score::DefaultGraphicsSliderImpl::mousePressEvent(*this, event);
}

void VSTGraphicsSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  score::DefaultGraphicsSliderImpl::mouseMoveEvent(*this, event);
}

void VSTGraphicsSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  score::DefaultGraphicsSliderImpl::mouseReleaseEvent(*this, event);
}

void VSTGraphicsSlider::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  score::DefaultGraphicsSliderImpl::mouseDoubleClickEvent(*this, event);
}

QRectF VSTGraphicsSlider::boundingRect() const
{
  return m_rect;
}

void VSTGraphicsSlider::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  static const QPen darkPen{skin.HalfDark.color()};
  static const QPen grayPen{skin.Gray.color()};
  painter->setPen(darkPen);
  painter->setBrush(skin.Dark);

  // Draw rect
  const auto srect = sliderRect();
  painter->drawRoundedRect(srect, 1, 1);

  // Draw text
  painter->setPen(grayPen);
  char str[256]{};
  fx->dispatcher(fx, effGetParamDisplay, num, 0, str, m_value);

#if defined(__linux__)
  static const auto dpi_adjust = widget->devicePixelRatioF() > 1 ? 0 : -2;
#elif defined(_MSC_VER)
  static const constexpr auto dpi_adjust = -4;
#else
  static const constexpr auto dpi_adjust = -2;
#endif

  painter->drawText(
      srect.adjusted(6, dpi_adjust, -6, 0), QString::fromUtf8(str),
      getHandleX() > srect.width() / 2 ? QTextOption()
                                       : QTextOption(Qt::AlignRight));

  // Draw handle
  painter->setBrush(skin.HalfLight);
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->drawRect(handleRect());
}

bool VSTGraphicsSlider::isInHandle(QPointF p)
{
  return handleRect().contains(p);
}

double VSTGraphicsSlider::getHandleX() const
{
  return 4 + sliderRect().width() * m_value;
}

QRectF VSTGraphicsSlider::sliderRect() const
{
  return m_rect.adjusted(4, 3, -4, -3);
}

QRectF VSTGraphicsSlider::handleRect() const
{
  return {getHandleX() - 4., 1., 8., m_rect.height() - 1};
}

VSTEffectItem::VSTEffectItem(
    const VSTEffectModel& effect,
    const score::DocumentContext& doc,
    score::RectItem* root)
    : score::EmptyRectItem{root}
{
  rootItem = root;
  using namespace Control::Widgets;
  QObject::connect(
      &effect, &Process::ProcessModel::controlAdded, this,
      [&](const Id<Process::Port>& id) {
        auto inlet = safe_cast<VSTControlInlet*>(effect.inlet(id));
        setupInlet(effect, *inlet, doc);
        rootItem->setRect(rootItem->childrenBoundingRect());
      });

  QObject::connect(
      &effect, &Process::ProcessModel::controlRemoved, this,
      [&](const Process::Port& port) {
        auto inlet = qobject_cast<const VSTControlInlet*>(&port);
        SCORE_ASSERT(inlet);
        auto it = ossia::find_if(
            controlItems, [&](auto p) { return p.first == inlet; });
        if (it != controlItems.end())
        {
          double pos_y = it->second->pos().y();
          delete it->second;
          it = controlItems.erase(it);
          for (; it != controlItems.end(); ++it)
          {
            auto rect = it->second;
            rect->setPos(0, pos_y);
            pos_y += rect->boundingRect().height();
          }
        }
        rootItem->setRect(rootItem->childrenBoundingRect());
      });

  {
    auto tempo = safe_cast<Process::ControlInlet*>(effect.inlets()[1]);
    setupInlet(TempoChooser(), *tempo, doc);
  }
  {
    auto sg = safe_cast<Process::ControlInlet*>(effect.inlets()[2]);
    setupInlet(TimeSigChooser(), *sg, doc);
  }
  for (std::size_t i = 3; i < effect.inlets().size(); i++)
  {
    auto inlet = safe_cast<VSTControlInlet*>(effect.inlets()[i]);
    setupInlet(effect, *inlet, doc);
  }
}

void VSTEffectItem::setupInlet(
    const VSTEffectModel& fx,
    VSTControlInlet& inlet,
    const score::DocumentContext& doc)
{
  auto rect = new score::EmptyRectItem{this};

  double pos_y = this->childrenBoundingRect().height();

  auto port_item = VSTControlPortFactory{}.makeItem(inlet, doc, rect, this);
  static const auto close_off = score::get_pixmap(":/icons/close_off.png");
  static const auto close_on = score::get_pixmap(":/icons/close_on.png");

  auto lab = new Scenario::SimpleTextItem{rect};
  lab->setColor(ScenarioStyle::instance().EventDefault);
  lab->setText(inlet.customData());
  lab->setPos(15, 2);

  double h = 20.;
  if (fx.fx)
  {
    QGraphicsItem* widg
        = VSTFloatSlider::make_item(fx.fx->fx, inlet, doc, nullptr, this);
    widg->setParentItem(rect);
    widg->setPos(15, lab->boundingRect().height());

    h = std::max(
        20., (qreal)(
                 widg->boundingRect().height() + lab->boundingRect().height()
                 + 2.));

    if (fx.fx->fx->numParams >= 10)
    {
      auto rm_item
          = new score::QGraphicsPixmapButton{close_on, close_off, rect};
      connect(
          rm_item, &score::QGraphicsPixmapButton::clicked, this,
          [&doc, &fx, id = inlet.id()] {
            QTimer::singleShot(0, [&doc, &fx, id] {
              CommandDispatcher<> disp{doc.commandStack};
              disp.submitCommand<RemoveVSTControl>(fx, id);
            });
          });

      rm_item->setPos(2., 2.3 * h / 4.);
    }
  }

  port_item->setPos(7., 1.3 * h / 4.);

  rect->setPos(0, pos_y);
  rect->setRect(QRectF{0., 0, 170., h});
  controlItems.push_back({&inlet, rect});
}

ERect VSTWindow::getRect(AEffect& e)
{
  ERect* vstRect{};

  e.dispatcher(&e, effEditGetRect, 0, 0, &vstRect, 0.f);

  int16_t w{};
  int16_t h{};
  if (vstRect)
  {
    w = vstRect->right - vstRect->left;
    h = vstRect->bottom - vstRect->top;
  }

  if (w <= 1)
    w = 640;
  if (h <= 1)
    h = 480;

  if (vstRect)
    return ERect{vstRect->top, vstRect->left, vstRect->bottom, vstRect->right};
  else
    return ERect{0, 0, w, h};
}

bool VSTWindow::hasUI(AEffect& e)
{
  return e.flags & VstAEffectFlags::effFlagsHasEditor;
}

VSTWindow::VSTWindow(const VSTEffectModel& e, const score::DocumentContext& ctx, QWidget* parent)
  : VSTWindow{e, ctx}
{
  setAttribute(Qt::WA_DeleteOnClose, true);
  if (!m_defaultWidg)
  {
    connect(
          &ctx.coarseUpdateTimer, &QTimer::timeout, this,
          [=] {
      if (auto eff = effect.lock())
        eff->fx->dispatcher(eff->fx, effEditIdle, 0, 0, nullptr, 0);
    },
    Qt::UniqueConnection);
  }

  bool ontop = ctx.app.settings<Media::Settings::Model>().getVstAlwaysOnTop();
  if(ontop)
  {
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
  }
  e.externalUIVisible(true);
}

VSTWindow::~VSTWindow()
{
}

void VSTWindow::closeEvent(QCloseEvent* event)
{
  QPointer<VSTWindow> p(this);
  if (auto eff = effect.lock())
    eff->fx->dispatcher(eff->fx, effEditClose, 0, 0, nullptr, 0);
  const_cast<QWidget*&>(m_model.externalUI) = nullptr;
  m_model.externalUIVisible(false);
  if (p)
    QDialog::closeEvent(event);
}

void VSTWindow::resizeEvent(QResizeEvent* event)
{
  // setup_rect(this, event->size().width(), event->size().height());
  QDialog::resizeEvent(event);
}

void VSTWindow::resize(int w, int h)
{
  setup_rect(this, w, h);
}

QGraphicsItem* VSTFloatSlider::make_item(
    AEffect* fx,
    VSTControlInlet& inlet,
    const score::DocumentContext& ctx,
    QWidget* parent,
    QObject* context)
{
  auto sl = new VSTGraphicsSlider{fx, inlet.fxNum, nullptr};
  sl->setRect({0., 0., 150., 15.});
  sl->setValue(ossia::convert<double>(inlet.value()));

  QObject::connect(
      sl, &VSTGraphicsSlider::sliderMoved, context, [=, &inlet, &ctx] {
        sl->moving = true;
        ctx.dispatcher.submitCommand<SetVSTControl>(inlet, sl->value());
      });
  QObject::connect(
      sl, &VSTGraphicsSlider::sliderReleased, context, [&ctx, sl]() {
        ctx.dispatcher.commit();
        sl->moving = false;
      });

  QObject::connect(&inlet, &VSTControlInlet::valueChanged, sl, [=](float val) {
    if (!sl->moving)
      sl->setValue(val);
  });

  return sl;
}
}
