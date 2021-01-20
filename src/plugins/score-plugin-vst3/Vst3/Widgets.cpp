#include "Widgets.hpp"
#include <Vst3/Control.hpp>
#include <Vst3/Commands.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Dataflow/Commands/CreateModulation.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Process/Style/Pixmaps.hpp>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/GraphicsSliderBaseImpl.hpp>
#include <score/widgets/Pixmap.hpp>

#include <QGraphicsScene>

#include <Effect/EffectLayout.hpp>
#include <Engine/Node/CommonWidgets.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(vst3::VSTGraphicsSlider)

namespace vst3
{

VSTGraphicsSlider::VSTGraphicsSlider(Steinberg::Vst::IEditController* fx, Steinberg::Vst::ParamID num, QGraphicsItem* parent)
    : QGraphicsSliderBase{parent}
{
  this->fx = fx;
  this->num = num;
  this->m_value = fx->getParamNormalized(num);
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
  Steinberg::Vst::String128 str;
  m_value = fx->getParamNormalized(num);
  fx->getParamStringByValue(num, m_value, str);

  score::DefaultGraphicsSliderImpl::paint(
      *this, score::Skin::instance(), QString::fromUtf16(str), painter, widget);
}

VSTEffectItem::VSTEffectItem(
    const Model& effect,
    const Process::Context& doc,
    QGraphicsItem* root)
    : score::EmptyRectItem{root}
{
  rootItem = root;

  if(!effect.fx)
    return;

  using namespace Control::Widgets;
  QObject::connect(
      &effect, &Process::ProcessModel::controlAdded, this, [&](const Id<Process::Port>& id) {
        auto inlet = safe_cast<ControlInlet*>(effect.inlet(id));
        setupInlet(effect, *inlet, doc);
      });

  QObject::connect(
      &effect, &Process::ProcessModel::controlRemoved, this, [&](const Process::Port& port) {
        auto inlet = qobject_cast<const ControlInlet*>(&port);
        SCORE_ASSERT(inlet);
        auto it = ossia::find_if(controlItems, [&](auto p) { return p.first == inlet; });
        if (it != controlItems.end())
        {
          delete it->second;
          it = controlItems.erase(it);
          int i = std::distance(controlItems.begin(), it);
          for (; it != controlItems.end(); ++it, ++i)
          {
            score::EmptyRectItem* rect = it->second;
            QPointF pos = Process::currentWigetPos(
                i, [&](int j) { return controlItems[j].second->boundingRect().size(); });

            rect->setPos(pos);
          }
        }
        updateRect();
      });

  for (auto inlet : effect.inlets())
  {
    if(auto inl = qobject_cast<ControlInlet*>(inlet))
       setupInlet(effect, *inl, doc);
  }
  updateRect();
}

void VSTEffectItem::setupInlet(
    const Model& fx,
    ControlInlet& inlet,
    const Process::Context& doc)
{
  if (!fx.fx)
    return;

  int i = controlItems.size();

  auto csetup = Process::controlSetup(
      [](auto& factory, auto& inlet, const auto& doc, auto item, auto parent) {
        return factory.makeItem(inlet, doc, item, parent);
      },
      [&](auto& factory, auto& inlet, const auto& doc, auto item, auto parent) {
        return VSTFloatSlider::make_item(fx.fx.controller, inlet, doc, item, parent);
      },
      [&](int j) { return controlItems[j].second->boundingRect().size(); },
      [&] { return inlet.customData(); },
      [](auto&&...) -> auto& {
        static VSTControlPortFactory f;
        return f;
      });

  // TODO useless, find a way to remove
  static const auto& portFactory = score::GUIAppContext().interfaces<Process::PortFactoryList>();
  auto ctl = Process::createControl(i, csetup, inlet, portFactory, doc, this, this);

  if (fx.fx.controller->getParameterCount() >= VST_DEFAULT_PARAM_NUMBER_CUTOFF)
  {
    const auto& pixmaps = Process::Pixmaps::instance();
    auto rm_item = new score::QGraphicsPixmapButton{pixmaps.close_on, pixmaps.close_off, ctl.item};
    connect(rm_item, &score::QGraphicsPixmapButton::clicked, this, [&doc, &fx, id = inlet.id()] {
      QTimer::singleShot(0, [&doc, &fx, id] {
        CommandDispatcher<> disp{doc.commandStack};
        // disp.submit<RemoveVSTControl>(fx, id);
      });
    });

    rm_item->setPos(8., 16.);
  }
  controlItems.push_back({&inlet, ctl.item});
  updateRect();
}

void VSTEffectItem::updateRect()
{
  QRectF cr = childrenBoundingRect();
  cr.setWidth(std::max(100., cr.width()));
  cr.setHeight(std::max(10., cr.height()));
  setRect(cr);
}

QWidget* VSTFloatSlider::make_widget(
    Steinberg::Vst::IEditController* fx,
    const ControlInlet& inlet,
    const score::DocumentContext& ctx,
    QWidget* parent,
    QObject* context)
{
  auto sl = new score::ValueDoubleSlider{parent};
  sl->min = 0.;
  sl->max = 1.;
  sl->setValue(ossia::convert<double>(inlet.value()));

  QObject::connect(sl, &score::ValueDoubleSlider::sliderMoved, context, [=, &inlet, &ctx] {
    sl->moving = true;
    ctx.dispatcher.submit<SetControl>(inlet, sl->value());
  });
  QObject::connect(sl, &score::ValueDoubleSlider::sliderReleased, context, [&ctx, sl]() {
    ctx.dispatcher.commit();
    sl->moving = false;
  });

  QObject::connect(&inlet, &ControlInlet::valueChanged, sl, [=](float val) {
    if (!sl->moving)
      sl->setValue(val);
  });

  return sl;
}
QGraphicsItem* VSTFloatSlider::make_item(
    Steinberg::Vst::IEditController* fx,
    ControlInlet& inlet,
    const score::DocumentContext& ctx,
    QGraphicsItem* parent,
    QObject* context)
{
  auto sl = new VSTGraphicsSlider{fx, inlet.fxNum, parent};
  sl->setValue(ossia::convert<double>(inlet.value()));

  QObject::connect(sl, &VSTGraphicsSlider::sliderMoved, context, [=, &inlet, &ctx] {
    sl->moving = true;
    ctx.dispatcher.submit<SetControl>(inlet, sl->value());
  });
  QObject::connect(sl, &VSTGraphicsSlider::sliderReleased, context, [&ctx, sl]() {
    ctx.dispatcher.commit();
    sl->moving = false;
  });

  QObject::connect(&inlet, &ControlInlet::valueChanged, sl, [=](float val) {
    if (!sl->moving)
      sl->setValue(val);
  });

  return sl;
}
}
/*
W_OBJECT_IMPL(vst3::VSTWindow)


#if defined(HAS_VST2)
#include "VSTWidgets.hpp"

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Dataflow/Commands/CreateModulation.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Media/Commands/VSTCommands.hpp>
#include <Media/Effect/Settings/Model.hpp>
#include <Media/Effect/VST/VSTControl.hpp>
#include <Process/Style/Pixmaps.hpp>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/GraphicsSliderBaseImpl.hpp>
#include <score/widgets/Pixmap.hpp>

#include <QGraphicsScene>

#include <Effect/EffectLayout.hpp>
#include <Engine/Node/CommonWidgets.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(vst3::VSTWindow)
W_OBJECT_IMPL(vst3::VSTGraphicsSlider)
ERect VSTWindow::getRect(Steinberg::Vst::IEditController& e)
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

bool VSTWindow::hasUI(Steinberg::Vst::IEditController& e)
{
  return e.flags & VstSteinberg::Vst::IEditControllerFlags::effFlagsHasEditor;
}

VSTWindow::VSTWindow(const Model& e, const score::DocumentContext& ctx, QWidget* parent)
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

VSTWindow::~VSTWindow() { }

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

}
#endif
*/
