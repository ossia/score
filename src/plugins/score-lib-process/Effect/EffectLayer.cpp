#include "EffectLayer.hpp"

#include <Process/ApplicationPlugin.hpp>
#include <Process/Commands/LoadPreset.hpp>
#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Dataflow/CableCopy.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <Process/Script/EditorOverlay.hpp>
#include <Process/Style/Pixmaps.hpp>
#include <Process/UIPlacement.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentView.hpp>
#include <core/view/CentralViewStack.hpp>
#include <core/view/FixedTabWidget.hpp>
#include <core/view/Window.hpp>

#include <ossia/detail/thread.hpp>

#include <QActionGroup>
#include <QDialogButtonBox>
#include <QMenu>
#include <QPointer>
#include <QToolButton>

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
    QObject::connect(
        ui_btn, &score::QGraphicsPixmapToggle::contextMenuRequested, self,
        [&effect, &context](QPoint pos) {
      QMenu menu;
      fillPlacementMenu(menu, effect, context, ProcessUIKind::ScriptEditor);
      menu.exec(pos);
    });
    return ui_btn;
  }
  return nullptr;
}

namespace
{
score::View* mainView() noexcept
{
  return qobject_cast<score::View*>(score::GUIAppContext().mainWindow);
}

score::CentralViewStack* centralViews(const score::DocumentContext& ctx) noexcept
{
  if(auto v = ctx.document.view())
    return &v->centralViews();
  return nullptr;
}

void showAsWindow(QWidget* win)
{
#if defined(__EMSCRIPTEN__)
  // Qt for wasm reports ShowIsFullScreen unconditionally, and QWidget::show()
  // acts on it exactly as QWindow::show() does: the window covers the page and
  // loses the frame Qt draws for it, leaving no way to close or move it.
  // Also, without an OS window manager the window would otherwise open
  // behind / unfocused: the first click on "Compile" would only activate it.
  win->setWindowFlag(Qt::WindowStaysOnTopHint, true);
  win->showNormal();
#else
  win->show();
#endif
  win->raise();
  win->activateWindow();
}

QString kindName(ProcessUIKind kind)
{
  return kind == ProcessUIKind::ScriptEditor ? QObject::tr("code") : QObject::tr("UI");
}

// "My script (code)", "My script (UI)": a process can have both open
QString processTitle(const Process::ProcessModel& proc, ProcessUIKind kind)
{
  auto n = proc.prettyName();
  if(n.isEmpty())
    n = proc.metadata().getLabel();
  return QStringLiteral("%1 (%2)").arg(n, kindName(kind));
}

/**
 * Bookkeeping attached to a process UI widget: where it is and the
 * connections that placement made, undone when it moves.
 * Deleted with the widget.
 */
class PlacedUI final : public QObject
{
public:
  PlacedUI(QWidget* win, UIPlacement p, ProcessUIKind k)
      : QObject{win}
      , placement{p}
      , kind{k}
  {
    setObjectName(QStringLiteral("PlacedUI"));
  }

  ~PlacedUI()
  {
    for(auto& c : connections)
      QObject::disconnect(c);
  }

  UIPlacement placement;
  ProcessUIKind kind;
  //! The widget put in the central stack: an overlay host, or the UI itself
  QPointer<QWidget> host;
  std::vector<QMetaObject::Connection> connections;
};

PlacedUI* placedState(QWidget* win)
{
  if(!win)
    return nullptr;
  for(auto child : win->children())
    if(auto state = dynamic_cast<PlacedUI*>(child))
      return state;
  return nullptr;
}

//! The placement asked for this process, or the user's default
UIPlacement placementFor(const Process::ProcessModel& proc, ProcessUIKind kind)
{
  switch(kind)
  {
    case ProcessUIKind::ScriptEditor:
      return UIPlacementSettings::fromString(
          proc.scriptEditorPlacement(), UIPlacementSettings::scriptEditorPlacement());
    case ProcessUIKind::ExternalUI: {
      // Only UIs made of plain widgets / Qt Quick can live inside the main
      // window; plug-ins owning a native window keep opening separately.
      if(!(proc.flags() & Process::ProcessFlags::ExternalUIEmbeddable))
        return UIPlacement::Window;
      return UIPlacementSettings::fromString(
          proc.processUIPlacement(), UIPlacementSettings::processUIPlacement());
    }
  }
  return UIPlacement::Window;
}

void showPlacementMenu(
    Process::ProcessModel& proc, const score::DocumentContext& ctx, ProcessUIKind kind,
    QPoint globalPos)
{
  QMenu menu;
  fillPlacementMenu(menu, proc, ctx, kind);
  menu.exec(globalPos);
}

//! A menu button in the editor's own button row, so that it can be moved
//! from wherever it is, including from a separate window.
void addPlacementButton(
    QWidget* win, Process::ProcessModel& proc, const score::DocumentContext& ctx,
    ProcessUIKind kind)
{
  auto bbox = win->findChild<QDialogButtonBox*>();
  if(!bbox)
    return;
  if(bbox->findChild<QToolButton*>(QStringLiteral("PlacementButton")))
    return;

  auto btn = new QToolButton{bbox};
  btn->setObjectName(QStringLiteral("PlacementButton"));
  btn->setAutoRaise(true);
  btn->setPopupMode(QToolButton::InstantPopup);
  btn->setIcon(makeIcons(
      QStringLiteral(":/icons/undock_on.png"), QStringLiteral(":/icons/undock_off.png"),
      QStringLiteral(":/icons/undock_off.png")));
  btn->setToolTip(QObject::tr("Where this editor opens"));
  score::setHelp(
      btn, QObject::tr(
               "Open the editor in a separate window, in the side panel or in "
               "the central view. Remembered for this process, and becomes "
               "the default for the others."));

  auto menu = new QMenu{btn};
  QObject::connect(menu, &QMenu::aboutToShow, btn, [menu, &proc, &ctx, kind] {
    // Rebuilt on every opening; the action groups are not among what
    // clear() deletes.
    qDeleteAll(menu->findChildren<QActionGroup*>(QString{}, Qt::FindDirectChildrenOnly));
    menu->clear();
    fillPlacementMenu(*menu, proc, ctx, kind);
  });
  btn->setMenu(menu);
  bbox->addButton(btn, QDialogButtonBox::HelpRole);
}

//! The document's background when it shows one (a Background device, a
//! watched texture port)
QWidget* backgroundPreview(const score::DocumentContext& ctx)
{
  if(auto docView = ctx.document.view())
  {
    auto& delegate = docView->viewDelegate();
    if(delegate.activeBackgroundRenderer())
      return new DocumentBackgroundPreview{delegate};
  }
  return nullptr;
}

//! What a central code editor is drawn over: the document's background, when
//! asked for and when the document shows one
QWidget* makePreview(const score::DocumentContext& ctx)
{
  switch(UIPlacementSettings::scriptEditorPreview())
  {
    case PreviewSource::Background:
      return backgroundPreview(ctx);
    case PreviewSource::None:
      return nullptr;
  }
  return nullptr;
}

/**
 * Puts a UI created for a process (script editor, custom UI) where the
 * user wants it. The widget is expected to delete itself on close
 * (Qt::WA_DeleteOnClose): the panels it gets docked into watch for that.
 */
void placeProcessUI(
    QWidget* win, Process::ProcessModel& proc, const score::DocumentContext& ctx,
    UIPlacement placement, ProcessUIKind kind)
{
  const char* icon = kind == ProcessUIKind::ScriptEditor ? "script" : "new_window";
  const auto title = [&proc, kind] { return processTitle(proc, kind); };

  delete placedState(win);
  auto state = new PlacedUI{win, placement, kind};
  state->host = win;

  win->setWindowTitle(title());
  if(kind == ProcessUIKind::ScriptEditor)
    addPlacementButton(win, proc, ctx, kind);

  switch(placement)
  {
    case UIPlacement::Central: {
      auto views = centralViews(ctx);
      if(!views)
        break;

      QWidget* host = win;
      if(kind == ProcessUIKind::ScriptEditor)
      {
        if(auto preview = makePreview(ctx))
        {
          host = new EditorOverlayHost{win, preview};
        }
      }
      state->host = host;

      views->addView(
          host, title(), QIcon{QStringLiteral(":/icons/%1_on.png").arg(icon)});
      views->showView(host);

      state->connections.push_back(
          QObject::connect(
              &proc.metadata(), &score::ModelMetadata::NameChanged, win,
              [host, title, v = QPointer<score::CentralViewStack>{views}] {
        if(v)
          v->setViewTitle(host, title());
      }));
      state->connections.push_back(
          QObject::connect(
              views, &score::CentralViewStack::viewContextMenuRequested, win,
              [host, &proc, &ctx, kind](QWidget* v, QPoint pos) {
        if(v == host)
          showPlacementMenu(proc, ctx, kind, pos);
      }));
      return;
    }
    case UIPlacement::SidePanel: {
      auto view = mainView();
      if(!view)
        break;

      auto act = view->addRightPanel(
          win, score::PanelStatus{
                   false, true, Qt::RightDockWidgetArea, -100000, title(), icon,
                   QKeySequence{}});
      view->showRightPanel(win);

      state->connections.push_back(
          QObject::connect(
              &proc.metadata(), &score::ModelMetadata::NameChanged, win,
              [win, act = QPointer<QAction>{act}, view = QPointer<score::View>{view},
               title] {
        if(!act)
          return;
        act->setText(title());
        act->setIconText(title());
        act->setToolTip(title());
        // The pane's title only follows the current tab when it is shown
        if(view && act->isChecked())
          view->showRightPanel(win);
      }));
      state->connections.push_back(
          QObject::connect(
              view->rightTabs, &score::FixedTabWidget::tabContextMenuRequested, win,
              [win, &proc, &ctx, kind](QWidget* tab, QPoint pos) {
        if(tab == win)
          showPlacementMenu(proc, ctx, kind, pos);
      }));
      return;
    }
    case UIPlacement::Window:
      break;
  }

  state->placement = UIPlacement::Window;
  showAsWindow(win);
}

//! Takes a process UI out of wherever placeProcessUI put it, without closing it
void detachProcessUI(QWidget* win, const score::DocumentContext& ctx)
{
  auto state = placedState(win);
  if(!state)
  {
    if(!win->isWindow())
      win->setParent(nullptr);
    return;
  }

  const QPointer<QWidget> host = state->host;
  switch(state->placement)
  {
    case UIPlacement::Central: {
      if(auto views = centralViews(ctx); views && host)
      {
        if(host == win)
        {
          views->removeView(win);
        }
        else if(auto overlay = qobject_cast<EditorOverlayHost*>(host.data()))
        {
          // The host closes itself once its editor is gone; its tab goes
          // now rather than with its deferred deletion
          if(overlay->editor() == win)
            overlay->releaseEditor();
          if(host)
            views->removeView(host);
        }
      }
      break;
    }
    case UIPlacement::SidePanel: {
      if(auto view = mainView())
        view->removeRightPanel(win);
      win->setParent(nullptr);
      break;
    }
    case UIPlacement::Window:
      win->hide();
      break;
  }
  delete state;
}

//! Brings back an already opened process UI in front of the user
void raiseProcessUI(QWidget* win, const score::DocumentContext& ctx)
{
  if(win->isWindow())
  {
    showAsWindow(win);
  }
  else if(auto views = centralViews(ctx))
  {
    if(auto view = views->viewContaining(win))
      views->showView(view);
    else if(auto main = mainView())
      main->showRightPanel(win);
  }
  else if(auto view = mainView())
  {
    view->showRightPanel(win);
  }
}

//! Closes a process UI, making sure it is gone when this returns.
//! True when the UI left the bookkeeping to us: the pointer was still set
//! after its close, and got reset here.
bool closeProcessUI(QWidget*& slot, const score::DocumentContext& ctx)
{
  auto win = slot;
  if(!win)
    return false;

  // close() lets the widget notify that it is going away (the script
  // dialogs and the plug-in windows reset the process's pointer themselves)
  // and, with WA_DeleteOnClose, schedules its deletion. UIs that only hide
  // on close (the Pd wrapper for instance) are deleted here.
  const QPointer<QWidget> alive = win;
  win->close();
  const bool resetHere = (slot == win);
  if(resetHere)
    slot = nullptr;
  if(!alive)
    return resetHere;

  // Out of its tab right away, not once the deferred deletion has run
  detachProcessUI(alive, ctx);
  if(alive && !alive->testAttribute(Qt::WA_DeleteOnClose))
    delete alive.data();
  return resetHere;
}
}

void setupScriptUI(
    Process::ProcessModel& proc, const Process::LayerFactory& fact,
    const score::DocumentContext& ctx, bool show)
{
  if(!show)
  {
    closeProcessUI(proc.scriptUI, ctx);
    return;
  }

  if(auto win = proc.scriptUI)
  {
    raiseProcessUI(win, ctx);
    return;
  }

  auto win = fact.makeScriptUI(proc, ctx, nullptr);
  if(!win)
    return;

  proc.scriptUI = win;
  win->setAttribute(Qt::WA_DeleteOnClose);

  placeProcessUI(
      win, proc, ctx, placementFor(proc, ProcessUIKind::ScriptEditor),
      ProcessUIKind::ScriptEditor);
  proc.scriptUIVisible(true);
}

void setupExternalUI(
    Process::ProcessModel& proc, const Process::LayerFactory& fact,
    const score::DocumentContext& ctx, bool show)
{
  if(!show)
  {
    // A UI that does not report its own closing (a window container: its
    // cleanup only runs with the deferred deletion, once the pointer is
    // gone) gets reported here.
    if(closeProcessUI(proc.externalUI, ctx))
      proc.externalUIVisible(false);
    return;
  }

  if(auto win = proc.externalUI)
  {
    raiseProcessUI(win, ctx);
    return;
  }

  auto win = fact.makeExternalUI(proc, ctx, nullptr);
  if(!win)
    return;

  proc.externalUI = win;
  placeProcessUI(
      win, proc, ctx, placementFor(proc, ProcessUIKind::ExternalUI),
      ProcessUIKind::ExternalUI);
  proc.externalUIVisible(true);
}

void moveProcessUI(
    Process::ProcessModel& proc, const score::DocumentContext& ctx, ProcessUIKind kind,
    const QString& placement)
{
  // What gets stored ends up in the document: only known names
  if(!placement.isEmpty() && !UIPlacementSettings::names().contains(placement))
    return;

  // An explicit choice is kept for this process and becomes the default
  // for the others; "default" makes this process follow it again.
  QWidget* win{};
  switch(kind)
  {
    case ProcessUIKind::ScriptEditor:
      proc.setScriptEditorPlacement(placement);
      if(!placement.isEmpty())
        UIPlacementSettings::setScriptEditorPlacement(
            UIPlacementSettings::fromString(
                placement, UIPlacementSettings::scriptEditorPlacement()));
      win = proc.scriptUI;
      break;
    case ProcessUIKind::ExternalUI:
      proc.setProcessUIPlacement(placement);
      if(!placement.isEmpty())
        UIPlacementSettings::setProcessUIPlacement(
            UIPlacementSettings::fromString(
                placement, UIPlacementSettings::processUIPlacement()));
      win = proc.externalUI;
      break;
  }
  if(!win)
    return;

  const auto target = placementFor(proc, kind);
  detachProcessUI(win, ctx);
  placeProcessUI(win, proc, ctx, target, kind);
}

void fillPlacementMenu(
    QMenu& menu, Process::ProcessModel& proc, const score::DocumentContext& ctx,
    ProcessUIKind kind)
{
  const bool isEditor = kind == ProcessUIKind::ScriptEditor;
  auto title = menu.addAction(
      isEditor ? QObject::tr("Open the editor in") : QObject::tr("Open the UI in"));
  title->setEnabled(false);

  if(!isEditor && !(proc.flags() & Process::ProcessFlags::ExternalUIEmbeddable))
  {
    auto note = menu.addAction(QObject::tr("This UI can only open in its own window"));
    note->setEnabled(false);
    return;
  }

  const QString& current
      = isEditor ? proc.scriptEditorPlacement() : proc.processUIPlacement();
  const auto userDefault = isEditor ? UIPlacementSettings::scriptEditorPlacement()
                                    : UIPlacementSettings::processUIPlacement();
  const auto displayName = [](UIPlacement p) {
    switch(p)
    {
      case UIPlacement::Window:
        return QObject::tr("Separate window");
      case UIPlacement::SidePanel:
        return QObject::tr("Side panel");
      case UIPlacement::Central:
        return QObject::tr("Central view");
    }
    return QString{};
  };

  auto group = new QActionGroup{&menu};
  group->setExclusive(true);
  const auto addChoice = [&](const QString& text, const QString& value) {
    auto act = menu.addAction(text);
    act->setCheckable(true);
    act->setChecked(value == current);
    group->addAction(act);
    QObject::connect(act, &QAction::triggered, &menu, [&proc, &ctx, kind, value] {
      moveProcessUI(proc, ctx, kind, value);
    });
  };

  addChoice(QObject::tr("Default (%1)").arg(displayName(userDefault)), QString{});
  const auto& names = UIPlacementSettings::names();
  for(int i = 0; i < names.size(); i++)
    addChoice(displayName(static_cast<UIPlacement>(i)), names[i]);

  if(!isEditor)
    return;

  // What the editor is drawn over in the central view
  menu.addSeparator();
  auto previewTitle = menu.addAction(QObject::tr("Behind the editor"));
  previewTitle->setEnabled(false);
  const auto previewName = [](PreviewSource p) {
    switch(p)
    {
      case PreviewSource::Background:
        return QObject::tr("Document background, without chrome");
      case PreviewSource::None:
        return QObject::tr("Nothing: a plain editor");
    }
    return QString{};
  };
  const auto currentPreview = UIPlacementSettings::scriptEditorPreview();
  auto previewGroup = new QActionGroup{&menu};
  previewGroup->setExclusive(true);
  for(auto src : {PreviewSource::Background, PreviewSource::None})
  {
    auto act = menu.addAction(previewName(src));
    act->setCheckable(true);
    act->setChecked(src == currentPreview);
    previewGroup->addAction(act);
    QObject::connect(act, &QAction::triggered, &menu, [&proc, &ctx, src] {
      UIPlacementSettings::setScriptEditorPreview(src);
      // Applied right away to an open central editor
      auto win = proc.scriptUI;
      auto state = placedState(win);
      if(!win || !state || state->placement != UIPlacement::Central)
        return;
      detachProcessUI(win, ctx);
      placeProcessUI(win, proc, ctx, UIPlacement::Central, ProcessUIKind::ScriptEditor);
    });
  }
}

void setupScriptUI(
    Process::ProcessModel& proc, const score::DocumentContext& ctx, bool show)
{
  if(!(proc.flags() & Process::ProcessFlags::ScriptEditingSupported))
    return;

  auto& facts = ctx.app.interfaces<Process::LayerFactoryList>();
  auto fact = facts.findDefaultFactory(proc);
  if(!fact)
    return;

  setupScriptUI(proc, *fact, ctx, show);
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
        QObject::tr(
            "Show/hide UI\nShow the process's interface for instance for VSTs "
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
    QObject::connect(
        ui_btn, &score::QGraphicsPixmapToggle::contextMenuRequested, self,
        [&effect, &context](QPoint pos) {
      QMenu menu;
      fillPlacementMenu(menu, effect, context, ProcessUIKind::ExternalUI);
      menu.exec(pos);
    });
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
          categoryMenu(p->category)->addAction(p->name, menu, [p, loadPreset] {
            loadPreset(*p);
          });
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
