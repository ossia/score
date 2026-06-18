#include "EffectLayer.hpp"

#include <Process/ApplicationPlugin.hpp>
#include <Process/Commands/LoadPreset.hpp>
#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Dataflow/CableCopy.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Process/Style/Pixmaps.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/widgets/HelpInteraction.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/thread.hpp>

#include <QHash>
#include <QMenu>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Process::EffectLayerPresenter)
namespace Process
{

EffectLayerView::EffectLayerView(QGraphicsItem* parent)
    : Process::LayerView{parent}
{
}

EffectLayerView::~EffectLayerView() { }

void EffectLayerView::setWidth(qreal val, qreal defaultWidth)
{
  m_defaultWidth = defaultWidth;
  LayerView::setWidth(val);
}

void EffectLayerView::paint_impl(QPainter*) const { }

EffectLayerPresenter::EffectLayerPresenter(
    const ProcessModel& model, Process::EffectLayerView* view, const Context& ctx,
    QObject* parent)
    : LayerPresenter{model, view, ctx, parent}
    , m_view{view}
{
  putToFront();
}

EffectLayerPresenter::~EffectLayerPresenter() { }

void EffectLayerPresenter::setWidth(qreal val, qreal defaultWidth)
{
  m_view->setWidth(val, defaultWidth);
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
    QMenu& menu, QPoint pos, QPointF scenepos, const LayerContextMenuManager& mgr)
{
}

QGraphicsItem* makeScriptButton(
    ProcessModel& effect, const score::DocumentContext& context, QObject* self,
    QGraphicsItem* root)
{
  auto& pixmaps = Process::Pixmaps::instance();
  auto& facts = context.app.interfaces<Process::LayerFactoryList>();
  auto fact = facts.findDefaultFactory(effect);
  if(effect.flags() & Process::ProcessFlags::ScriptEditingSupported)
  {
    auto ui_btn = new score::QGraphicsPixmapToggle{
        pixmaps.show_script_on, pixmaps.show_script_off, root};
    ui_btn->setToolTip(
        QObject::tr("Show/hide UI\nShow the process's script editor for JS, shaders, etc."));
    QObject::connect(
        ui_btn, &score::QGraphicsPixmapToggle::toggled, self,
        [=, &effect, &context](bool b) {
      Process::setupScriptUI(effect, *fact, context, b);
    });

    if(effect.scriptUI)
      ui_btn->setState(true);
    QObject::connect(
        &effect, &Process::ProcessModel::scriptUIVisible, ui_btn,
        [=](bool v) { ui_btn->setState(v); });
    return ui_btn;
  }
  return nullptr;
}

void setupScriptUI(
    Process::ProcessModel& proc, const Process::LayerFactory& fact,
    const score::DocumentContext& ctx, bool show)
{
  if(show)
  {
    if(proc.scriptUI)
      return;

    if(auto win = fact.makeScriptUI(proc, ctx, nullptr))
    {
      const_cast<QWidget*&>(proc.scriptUI) = win;
      win->show();
    }
  }
  else
  {
    if(auto win = proc.scriptUI)
    {
      win->close();
      delete win;
      const_cast<QWidget*&>(proc.scriptUI) = nullptr;
    }
  }
}

void setupExternalUI(
    Process::ProcessModel& proc, const Process::LayerFactory& fact,
    const score::DocumentContext& ctx, bool show)
{
  if(show)
  {
    if(proc.externalUI)
      return;

    if(auto win = fact.makeExternalUI(proc, ctx, nullptr))
    {
      const_cast<QWidget*&>(proc.externalUI) = win;
      win->show();
    }
  }
  else
  {
    if(auto win = proc.externalUI)
    {
      win->close();
      delete win;
      const_cast<QWidget*&>(proc.externalUI) = nullptr;
    }
  }
}

void setupExternalUI(
    Process::ProcessModel& proc, const score::DocumentContext& ctx, bool show)
{
  auto& facts = ctx.app.interfaces<Process::LayerFactoryList>();

  auto fact = facts.findDefaultFactory(proc);
  if(!fact || !fact->hasExternalUI(proc, ctx))
    return;

  setupExternalUI(proc, *fact, ctx, show);
}

QGraphicsItem* makeExternalUIButton(
    ProcessModel& effect, const score::DocumentContext& context, QObject* self,
    QGraphicsItem* root)
{
  auto& pixmaps = Process::Pixmaps::instance();
  auto& facts = context.app.interfaces<Process::LayerFactoryList>();
  auto fact = facts.findDefaultFactory(effect);
  if(fact && fact->hasExternalUI(effect, context))
  {
    auto ui_btn = new score::QGraphicsPixmapToggle{
        pixmaps.show_ui_on, pixmaps.show_ui_off, root};
    ui_btn->setToolTip(
        QObject::tr("Show/hide UI\nShow the process's interface for instance for VSTs "
                    "or script editor for JS, etc."));
    QObject::connect(
        ui_btn, &score::QGraphicsPixmapToggle::toggled, self,
        [=, &effect, &context](bool b) {
      Process::setupExternalUI(effect, *fact, context, b);
        });

    if(effect.externalUI)
      ui_btn->setState(true);
    QObject::connect(
        &effect, &Process::ProcessModel::externalUIVisible, ui_btn,
        [=](bool v) { ui_btn->setState(v); });
    return ui_btn;
  }
  return nullptr;
}

// Order presets the way the menu displays them: grouped by category so each
// submenu's entries stay together, then alphabetically by name within a category.
static bool
presetMenuOrder(const Process::Preset* lhs, const Process::Preset* rhs) noexcept
{
  if(lhs->category != rhs->category)
    return lhs->category < rhs->category;
  return lhs->name < rhs->name;
}

score::QGraphicsDraggablePixmap* makePresetButton(
    const ProcessModel& proc, const score::DocumentContext& context, QObject* self,
    QGraphicsItem* root)
{
  auto& pixmaps = Process::Pixmaps::instance();
  auto ui_btn
      = new score::QGraphicsDraggablePixmap{pixmaps.preset_on, pixmaps.preset_off, root};
  ui_btn->setToolTip(
      QObject::tr(
          "Presets\nDrag to the library to save the current preset. If there "
          "are existing presets, they will be shown in a menu."));
  ui_btn->createDrag = [&proc](QMimeData& mime) {
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
    mime.setData(score::mime::processpreset(), proc.savePreset().toJson());
  };

  ui_btn->click = [&proc, &context](Qt::MouseButton btn, QPointF screenPos) {
    auto& pplug = context.app.applicationPlugin<Process::ApplicationPlugin>();
    const auto& presets = pplug.presets;

    switch(btn)
    {
      default:
        break;
      case Qt::LeftButton: {
        // Preset menu
        auto menu = new QMenu;
        menu->addAction(
            "Save current preset", menu, [&proc, &pplug] { pplug.savePreset(&proc); });

        auto loadPreset = [&proc, &context](const Process::Preset& preset) {
          auto& load_preset_ifaces
              = context.app.interfaces<LoadPresetCommandFactoryList>();

          auto cmd = load_preset_ifaces.make(
              &LoadPresetCommandFactory::make, proc, preset, context);
          if(cmd)
          {
            CommandDispatcher<> c{context.commandStack};
            c.submit(cmd);
          }
        };

        // Resolve a "Foo/Bar/Baz" category path to the submenu it designates,
        // creating (and caching) the intermediate submenus on the way.
        // An empty category resolves to the root menu.
        QHash<QString, QMenu*> submenus;
        auto categoryMenu = [&](const QString& category) -> QMenu* {
          QMenu* current = menu;
          QString path;
          const auto parts = category.split('/', Qt::SkipEmptyParts);
          for(const QString& rawPart : parts)
          {
            const QString part = rawPart.trimmed();
            if(part.isEmpty())
              continue;
            if(!path.isEmpty())
              path += '/';
            path += part;

            auto it = submenus.find(path);
            if(it != submenus.end())
              current = it.value();
            else
              current = *submenus.insert(path, current->addMenu(part));
          }
          return current;
        };

        std::vector<const Process::Preset*> goodPresets;
        const auto& k = proc.concreteKey();
        const auto& e = proc.effect();
        for(auto& preset : presets)
        {
          if(preset.key.key == k && preset.key.effect == e)
            goodPresets.push_back(&preset);
        }
        std::sort(goodPresets.begin(), goodPresets.end(), presetMenuOrder);

        menu->addSeparator();

        for(auto p : goodPresets)
        {
          categoryMenu(p->category)
              ->addAction(p->name, menu, [p, loadPreset] { loadPreset(*p); });
        }

        if(auto proc_builtins = proc.builtinPresets(); !proc_builtins.empty())
        {
          if(!goodPresets.empty())
            menu->addSeparator();

          for(auto& p : proc_builtins)
          {
            // FIXME try to understand why just p.name does not work here
            categoryMenu(p.category)
                ->addAction("" + p.name, menu, [p = std::move(p), loadPreset] {
              loadPreset(p);
            });
          }
        }

        menu->exec(screenPos.toPoint());
        menu->deleteLater();
        break;
      }

      case Qt::ForwardButton:
      case Qt::BackButton: {
        std::vector<const Process::Preset*> goodPresets;
        const auto& k = proc.concreteKey();
        const auto& e = proc.effect();
        for(auto& preset : presets)
          if(preset.key.key == k && preset.key.effect == e)
            goodPresets.push_back(&preset);
        auto bps = proc.builtinPresets();
        for(auto& bp : bps)
          goodPresets.push_back(&bp);

        // Cycle in the same order the menu displays them.
        std::sort(goodPresets.begin(), goodPresets.end(), presetMenuOrder);

        if(goodPresets.size() < 2)
          return;

        // loadPreset() renames the process to the preset's name, so the
        // currently-loaded preset is the one whose name matches.
        const QString cur = proc.metadata().getName();
        int i = -1;
        for(int j = 0; j < std::ssize(goodPresets); j++)
        {
          if(goodPresets[j]->name == cur)
          {
            i = j;
            break;
          }
        }

        const int n = std::ssize(goodPresets);
        if(i < 0)
          // Nothing matches (renamed / never loaded): start at the relevant end.
          i = (btn == Qt::ForwardButton) ? 0 : (n - 1);
        else if(btn == Qt::ForwardButton)
          i = (i + 1) % n;
        else
          i = (i - 1 + n) % n;

        auto& load_preset_ifaces
            = context.app.interfaces<LoadPresetCommandFactoryList>();

        auto cmd = load_preset_ifaces.make(
            &LoadPresetCommandFactory::make, proc, *goodPresets[i], context);
        if(cmd)
        {
          CommandDispatcher<> c{context.commandStack};
          c.submit(cmd);
        }
        break;
      }
    }
  };

  return ui_btn;
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
