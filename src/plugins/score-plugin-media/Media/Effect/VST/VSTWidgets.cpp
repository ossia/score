#if defined(HAS_VST2)
#include "VSTWidgets.hpp"

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Dataflow/Commands/CreateModulation.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Dataflow/UI/PortItem.hpp>
#include <Media/Commands/VSTCommands.hpp>
#include <Media/Effect/Settings/Model.hpp>
#include <Media/Effect/VST/VSTControl.hpp>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/GraphicsSliderBaseImpl.hpp>
#include <score/widgets/Pixmap.hpp>

#include <QAction>
#include <QGraphicsScene>
#include <QMenu>

#include <Engine/Node/CommonWidgets.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Media::VST::VSTWindow)
W_OBJECT_IMPL(Media::VST::VSTGraphicsSlider)
namespace Media::VST
{

VSTGraphicsSlider::VSTGraphicsSlider(
    AEffect* fx,
    int num,
    QGraphicsItem* parent)
    : QGraphicsSliderBase{parent}
{
  this->fx = fx;
  this->num = num;
  this->m_value = fx->getParameter(fx, num);
  this->setAcceptedMouseButtons(Qt::LeftButton);
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

void VSTGraphicsSlider::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  char str[256]{};
  fx->dispatcher(fx, effGetParamDisplay, num, 0, str, m_value);
  score::DefaultGraphicsSliderImpl::paint(
      *this, score::Skin::instance(), QString::fromUtf8(str), painter, widget);
}

VSTEffectItem::VSTEffectItem(
    const VSTEffectModel& effect,
    const score::DocumentContext& doc,
    QGraphicsItem* root)
    : score::EmptyRectItem{root}
{
  rootItem = root;
  using namespace Control::Widgets;
  QObject::connect(
      &effect,
      &Process::ProcessModel::controlAdded,
      this,
      [&](const Id<Process::Port>& id) {
        auto inlet = safe_cast<VSTControlInlet*>(effect.inlet(id));
        setupInlet(effect, *inlet, doc);
        sizeChanged(rootItem->childrenBoundingRect().size());
      });

  QObject::connect(
      &effect,
      &Process::ProcessModel::controlRemoved,
      this,
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
        sizeChanged(rootItem->childrenBoundingRect().size());
      });

  //{
  //  auto tempo = safe_cast<Process::ControlInlet*>(effect.inlets()[1]);
  //  setupInlet(TempoChooser(), *tempo, doc);
  //}
  //{
  //  auto sg = safe_cast<Process::ControlInlet*>(effect.inlets()[2]);
  //  setupInlet(TimeSigChooser(), *sg, doc);
  //}
  for (std::size_t i = 3; i < effect.inlets().size(); i++)
  {
    auto inlet = safe_cast<VSTControlInlet*>(effect.inlets()[i]);
    setupInlet(effect, *inlet, doc);
  }
}

static const constexpr int MaxRows = 4;
double VSTEffectItem::currentColumnX() const
{
  int N = MaxRows * (controlItems.size() / MaxRows);
  qreal x = 0;
  for(int i = 0; i < N; )
  {
    qreal w = 0;
    for(int j = i; j < i + MaxRows && j < N; j++)
    {
      w = std::max(w, controlItems[j].second->boundingRect().width());
    }
    x += w;
    i += MaxRows;
  }
  return x;
}

void VSTEffectItem::setupInlet(
    const VSTEffectModel& fx,
    VSTControlInlet& inlet,
    const score::DocumentContext& doc)
{
  int i = controlItems.size();
  int row = i % MaxRows;
  auto item = new score::EmptyRectItem{this};

  auto port_item = VSTControlPortFactory{}.makeItem(inlet, doc, item, this);
  static const auto close_off = score::get_pixmap(":/icons/close_off.png");
  static const auto close_on = score::get_pixmap(":/icons/close_on.png");

  auto lab = new score::SimpleTextItem{score::Skin::instance().Port2.main, item};
  lab->setText(inlet.customData());
  lab->setPos(20., 2.);
  const qreal labelHeight = 10;

  double h = 20.;
  if (fx.fx)
  {
    QGraphicsItem* widg
        = VSTFloatSlider::make_item(fx.fx->fx, inlet, doc, nullptr, this);
    widg->setParentItem(item);
    widg->setPos(18., labelHeight + 5.);

    h = std::max(
        20.,
        (qreal)(
            widg->boundingRect().height() + lab->boundingRect().height()
            + 2.));

    if (fx.fx->fx->numParams >= 10)
    {
      auto rm_item
          = new score::QGraphicsPixmapButton{close_on, close_off, item};
      connect(
          rm_item,
          &score::QGraphicsPixmapButton::clicked,
          this,
          [&doc, &fx, id = inlet.id()] {
            QTimer::singleShot(0, [&doc, &fx, id] {
              CommandDispatcher<> disp{doc.commandStack};
              disp.submit<RemoveVSTControl>(fx, id);
            });
          });

      rm_item->setPos(8., 16.);
    }

    port_item->setPos(8., 4.);

    const qreal labelWidth = lab->boundingRect().width();
    const auto wrect = widg->boundingRect();
    const qreal widgetHeight = wrect.height();
    const qreal widgetWidth = wrect.width();
    auto w = std::max(90., std::max(25. + labelWidth, widgetWidth));
    const auto itemRect = QRectF{0., 0, w, h};
    item->setPos(currentColumnX(), row * h);
    item->setRect(itemRect);
  }

  controlItems.push_back({&inlet, item});
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

VSTWindow::VSTWindow(
    const VSTEffectModel& e,
    const score::DocumentContext& ctx,
    QWidget* parent)
    : VSTWindow{e, ctx}
{
  setAttribute(Qt::WA_DeleteOnClose, true);
  if (!m_defaultWidg)
  {
    connect(
        &ctx.coarseUpdateTimer,
        &QTimer::timeout,
        this,
        [=] {
          if (auto eff = effect.lock())
            eff->fx->dispatcher(eff->fx, effEditIdle, 0, 0, nullptr, 0);
        },
        Qt::UniqueConnection);
  }

  bool ontop = ctx.app.settings<Media::Settings::Model>().getVstAlwaysOnTop();
  if (ontop)
  {
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
  }
  e.externalUIVisible(true);
}

VSTWindow::~VSTWindow() {}

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

QWidget* VSTFloatSlider::make_widget(
    AEffect* fx,
    VSTControlInlet& inlet,
    const score::DocumentContext& ctx,
    QWidget* parent,
    QObject* context)
{
  auto sl = new score::ValueDoubleSlider{parent};
  sl->min = 0.;
  sl->max = 1.;
  sl->setValue(ossia::convert<double>(inlet.value()));

  QObject::connect(
      sl, &score::ValueDoubleSlider::sliderMoved, context, [=, &inlet, &ctx] {
        sl->moving = true;
        ctx.dispatcher.submit<SetVSTControl>(inlet, sl->value());
      });
  QObject::connect(
      sl, &score::ValueDoubleSlider::sliderReleased, context, [&ctx, sl]() {
        ctx.dispatcher.commit();
        sl->moving = false;
      });

  QObject::connect(&inlet, &VSTControlInlet::valueChanged, sl, [=](float val) {
    if (!sl->moving)
      sl->setValue(val);
  });

  return sl;
}
QGraphicsItem* VSTFloatSlider::make_item(
    AEffect* fx,
    VSTControlInlet& inlet,
    const score::DocumentContext& ctx,
    QGraphicsItem* parent,
    QObject* context)
{
  auto sl = new VSTGraphicsSlider{fx, inlet.fxNum, parent};
  sl->setValue(ossia::convert<double>(inlet.value()));

  QObject::connect(
      sl, &VSTGraphicsSlider::sliderMoved, context, [=, &inlet, &ctx] {
        sl->moving = true;
        ctx.dispatcher.submit<SetVSTControl>(inlet, sl->value());
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
#endif
