#include "EffectLayer.hpp"

#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Process.hpp>
#include <Process/Style/Pixmaps.hpp>
#include <Process/ApplicationPlugin.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Process/Dataflow/CableCopy.hpp>

#include <score/graphics/GraphicWidgets.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <core/document/Document.hpp>

#include <ossia/detail/thread.hpp>
#include <QMenu>
#include <Process/Commands/LoadPreset.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Process::EffectLayerPresenter)
namespace Process
{

EffectLayerView::EffectLayerView(QGraphicsItem* parent)
    : Process::LayerView{parent}
{
}

EffectLayerView::~EffectLayerView() { }

void EffectLayerView::paint_impl(QPainter*) const { }

void EffectLayerView::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  if (ev->button() == Qt::RightButton)
  {
    askContextMenu(ev->screenPos(), ev->scenePos());
  }
  else
  {
    pressed(ev->scenePos());
  }
  ev->accept();
}

void EffectLayerView::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
}

void EffectLayerView::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
}

EffectLayerPresenter::EffectLayerPresenter(
    const ProcessModel& model,
    Process::LayerView* view,
    const Context& ctx,
    QObject* parent)
    : LayerPresenter{model, view, ctx, parent}
    , m_view{view}
{
  putToFront();
}

EffectLayerPresenter::~EffectLayerPresenter() { }

void EffectLayerPresenter::setWidth(qreal val, qreal defaultWidth)
{
  m_view->setWidth(val);
}

void EffectLayerPresenter::setHeight(qreal val)
{
  m_view->setHeight(val);
}

void EffectLayerPresenter::putToFront()
{
  m_view->setVisible(true);
}

void EffectLayerPresenter::putBehind()
{
  m_view->setVisible(false);
}

void EffectLayerPresenter::on_zoomRatioChanged(ZoomRatio) { }

void EffectLayerPresenter::parentGeometryChanged() { }

void EffectLayerPresenter::fillContextMenu(
    QMenu& menu,
    QPoint pos,
    QPointF scenepos,
    const LayerContextMenuManager& mgr)
{
}

void setupExternalUI(
    const Process::ProcessModel& proc,
    const Process::LayerFactory& fact,
    const score::DocumentContext& ctx,
    bool show)
{
  if (show)
  {
    if (proc.externalUI)
      return;

    if (auto win = fact.makeExternalUI(proc, ctx, nullptr))
    {
      const_cast<QWidget*&>(proc.externalUI) = win;
      win->show();
    }
  }
  else
  {
    if (auto win = proc.externalUI)
    {
      win->close();
      delete win;
      const_cast<QWidget*&>(proc.externalUI) = nullptr;
    }
  }
}

void setupExternalUI(
    const Process::ProcessModel& proc,
    const score::DocumentContext& ctx,
    bool show)
{
  auto& facts = ctx.app.interfaces<Process::LayerFactoryList>();

  auto fact = facts.findDefaultFactory(proc);
  if (!fact || !fact->hasExternalUI(proc, ctx))
    return;

  setupExternalUI(proc, *fact, ctx, show);
}

QGraphicsItem* makeExternalUIButton(
    const ProcessModel& effect,
    const score::DocumentContext& context,
    QObject* self,
    QGraphicsItem* root)
{
  auto& pixmaps = Process::Pixmaps::instance();
  auto& facts = context.app.interfaces<Process::LayerFactoryList>();
  auto fact = facts.findDefaultFactory(effect);
  if (fact && fact->hasExternalUI(effect, context))
  {
    auto ui_btn = new score::QGraphicsPixmapToggle{
        pixmaps.show_ui_on, pixmaps.show_ui_off, root};
    ui_btn->setToolTip(QObject::tr("Show/hide UI\nShow the process's interface for instance for VSTs or script editor for JS, etc."));
    QObject::connect(
        ui_btn,
        &score::QGraphicsPixmapToggle::toggled,
        self,
        [=, &effect, &context](bool b) {
          Process::setupExternalUI(effect, *fact, context, b);
        });

    if (effect.externalUI)
      ui_btn->setState(true);
    QObject::connect(
        &effect,
        &Process::ProcessModel::externalUIVisible,
        ui_btn,
        [=](bool v) { ui_btn->setState(v); });
    return ui_btn;
  }
  return nullptr;
}

score::QGraphicsDraggablePixmap* makePresetButton(
    const ProcessModel& proc,
    const score::DocumentContext& context,
    QObject* self,
    QGraphicsItem* root)
{
  auto& pixmaps = Process::Pixmaps::instance();
  {
    auto ui_btn = new score::QGraphicsDraggablePixmap{
        pixmaps.preset_on, pixmaps.preset_off, root};
    ui_btn->setToolTip(QObject::tr("Presets\nDrag to the library to save the current preset. If there are existing presets, they will be shown in a menu."));
    ui_btn->createDrag = [&proc] (QMimeData& mime) {

      QByteArray data;
      {
        JSONReader r;
        r.stream.StartObject();
        copyProcess(r, proc);
        r.obj["Path"] = score::IDocument::path(proc);
        r.obj["View"] = QStringLiteral("Nodal");
        r.stream.EndObject();
        data = r.toByteArray();
      }

      mime.setData(score::mime::layerdata(), data);
    };

    ui_btn->click = [&proc, &context] (QPointF screenPos) {
      auto& presets = context.app.applicationPlugin<Process::ApplicationPlugin>().presets;

      std::vector<Process::Preset*> goodPresets;
      const auto& k = proc.concreteKey();
      const auto& e = proc.effect();
      for(auto& preset : presets)
      {
        if(preset.key.key == k && preset.key.effect == e)
          goodPresets.push_back(&preset);
      }

      auto menu = new QMenu;
      for(auto p : goodPresets)
      {
        menu->addAction(p->name, menu, [p, &proc, &context] {
          CommandDispatcher<> c{context.commandStack};
          c.submit(new LoadPreset{proc, *p});
        });
      }
      menu->exec(screenPos.toPoint());
      menu->deleteLater();
    };

    return ui_btn;
  }
  return nullptr;
}

void copyProcess(JSONReader& r, const Process::ProcessModel& proc)
{
  const auto& ctx = score::IDocument::documentContext(proc);

  std::vector<const Process::ProcessModel*> vp{&proc};
  std::vector<Path<Process::ProcessModel>> vpath{proc};
  // Object is not created here but in SlotHeader
  r.obj["PID"] = ossia::get_pid();
  r.obj["Document"] = ctx.document.id();
  r.obj["Process"] = proc;
  r.obj["Cables"] = Process::cablesToCopy(vp, vpath, ctx);
}
}
