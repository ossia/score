// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ObjectMenuActions.hpp"

#include "ScenarioCopy.hpp"
#include "TextDialog.hpp"

#include <Process/Dataflow/CableCopy.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Commands/Cohesion/DoForSelectedIntervals.hpp>
#include <Scenario/Commands/Interval/InsertContentInInterval.hpp>
#include <Scenario/Commands/Scenario/Encapsulate.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteContent.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElements.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElementsAfter.hpp>
#include <Scenario/Commands/State/InsertContentInState.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Document/ScenarioEditor.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/Algorithms/ContainersAccessors.hpp>
#include <Scenario/Process/ScenarioGlobalCommandManager.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioSelection.hpp>
#include <Scenario/Process/ScenarioView.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/actions/Menu.hpp>
#include <score/actions/MenuManager.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/selection/Selectable.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/std/Optional.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentView.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QAction>
#include <QClipboard>
#include <QGraphicsView>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <qnamespace.h>

namespace Scenario
{

ObjectMenuActions::ObjectMenuActions(ScenarioApplicationPlugin* parent)
    : m_parent{parent}
    , m_eventActions{parent}
    , m_cstrActions{parent}
{
  if(!parent->context.applicationSettings.gui)
    return;
  using namespace score;

  // REMOVE - todo, put me in "core application plugin" instead
  m_removeElements = new QAction{tr("Remove selected elements"), this};
  // NOTE : the shortcut is defined in ScenarioActions.hpp
  m_removeElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  connect(
      m_removeElements, &QAction::triggered, this,
      &ObjectMenuActions::removeSelectedElements);

  // COPY/CUT
  m_copyContent = new QAction{tr("Copy"), this};
  m_copyContent->setShortcut(QKeySequence::Copy);
  m_copyContent->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  connect(m_copyContent, &QAction::triggered, this, [this]() {
    if(!isFocusingScenario())
      return;

    JSONReader r;
    copySelectedElementsToJson(r);
    if(r.empty())
      return;

    auto clippy = QApplication::clipboard();
    clippy->setText(r.toString());
  });

  m_cutContent = new QAction{tr("Cut"), this};
  m_cutContent->setShortcut(QKeySequence::Cut);
  m_cutContent->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  connect(m_cutContent, &QAction::triggered, this, [this] {
    if(!isFocusingScenario())
      return;

    JSONReader r;
    cutSelectedElementsToJson(r);
    if(r.empty())
      return;

    auto clippy = QApplication::clipboard();
    clippy->setText(r.toString());
  });

  m_pasteElements = new QAction{tr("Paste elements"), this};
  m_pasteElements->setShortcut(QKeySequence::Paste);
  m_pasteElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  connect(
      m_pasteElements, &QAction::triggered, this,
      qOverload<>(&ObjectMenuActions::pasteElements));

  m_pasteElementsAfter = new QAction{tr("Paste (after)"), this};
  connect(m_pasteElementsAfter, &QAction::triggered, [this] {
    if(!isFocusingScenario())
      return;

    auto pres = m_parent->focusedPresenter();
    if(!pres)
      return;
    auto views = pres->view().scene()->views();
    if(views.empty())
      return;

    auto view = views.front();

    QPoint pos = QCursor::pos();

    auto scene_pt = view->mapToScene(view->mapFromGlobal(pos));
    ScenarioView& sv = pres->view();
    auto sv_pt = sv.mapFromScene(scene_pt);
    if(!sv.contains(sv_pt))
    {
      sv_pt = sv.mapToScene(sv.boundingRect().center());
    }
    pasteElementsAfter(readJson(QApplication::clipboard()->text().toUtf8()));
  });

  // DISPLAY JSON
  m_elementsToJson = new QAction{tr("Convert selection to JSON"), this};
  connect(m_elementsToJson, &QAction::triggered, [this] {
    JSONReader r;
    copySelectedElementsToJson(r);
    TextDialog s{r.toString(), qApp->activeWindow()};

    s.exec();
  });

  // MERGE TIMESYNC
  m_mergeTimeSyncs = new QAction{this};
  connect(m_mergeTimeSyncs, &QAction::triggered, [this]() {
    auto sm = focusedScenarioModel(m_parent->currentDocument()->context());
    SCORE_ASSERT(sm);

    Scenario::mergeTimeSyncs(*sm, m_parent->currentDocument()->context().commandStack);
  });

  // Merge events
  m_mergeEvents = new QAction{this};
  connect(m_mergeEvents, &QAction::triggered, [this]() {
    auto sm = focusedScenarioModel(m_parent->currentDocument()->context());
    if(!sm)
      return;

    Scenario::mergeEvents(*sm, m_parent->currentDocument()->context().commandStack);
  });

  // Encapsulate
  m_encapsulate = new QAction{this};
  connect(m_encapsulate, &QAction::triggered, [this]() {
    auto sm = focusedScenarioModel(m_parent->currentDocument()->context());
    SCORE_ASSERT(sm);

    Scenario::EncapsulateInScenario(
        *sm, m_parent->currentDocument()->context().commandStack);
  });

  // Decapsulate
  m_decapsulate = new QAction{this};
  connect(m_decapsulate, &QAction::triggered, [this]() {
    auto sm = focusedScenarioModel(m_parent->currentDocument()->context());
    SCORE_ASSERT(sm);

    Scenario::DecapsulateScenario(
        *sm, m_parent->currentDocument()->context().commandStack);
  });

  // Duplicate
  m_duplicate = new QAction{this};
  connect(m_duplicate, &QAction::triggered, [this]() {
    auto sm = focusedScenarioModel(m_parent->currentDocument()->context());
    SCORE_ASSERT(sm);

    Scenario::Duplicate(*sm, m_parent->currentDocument()->context().commandStack);
  });

  // Selection actions
  m_selectAll = new QAction{tr("Select all"), this};
  connect(m_selectAll, &QAction::triggered, [this]() {
    if(!isFocusingScenario())
      return;
    if(auto pres = getScenarioDocPresenter())
      pres->selectAll();
  });

  m_deselectAll = new QAction{tr("Deselect all"), this};
  connect(m_deselectAll, &QAction::triggered, [this]() {
    if(!isFocusingScenario())
      return;
    if(auto pres = getScenarioDocPresenter())
      pres->deselectAll();
  });

  m_selectTop = new QAction{this};
  connect(m_selectTop, &QAction::triggered, [this] {
    if(!isFocusingScenario())
      return;
    if(auto pres = getScenarioDocPresenter())
      pres->selectTop();
  });

  m_goToParent = new QAction{this};
  connect(m_goToParent, &QAction::triggered, [this]() {
    if(!isFocusingScenario())
      return;
    if(auto pres = getScenarioDocPresenter())
    {
      auto* cur = (QObject*)&pres->displayedInterval();
      while((cur = cur->parent()))
      {
        if(auto parent = qobject_cast<Scenario::IntervalModel*>(cur))
        {
          pres->setDisplayedInterval(parent);
          break;
        }
      }
    }
  });
}

void ObjectMenuActions::makeGUIElements(score::GUIElements& e)
{
  using namespace score;
  auto& actions = e.actions;
  auto& base_menus = m_parent->context.menus.get();

  auto& scenariomodel_cond
      = m_parent->context.actions.condition<EnableWhenScenarioModelObject>();
  auto& scenarioiface_cond
      = m_parent->context.actions.condition<EnableWhenScenarioInterfaceObject>();

  auto& scenariodocument_cond
      = m_parent->context.actions
            .condition<score::EnableWhenDocumentIs<Scenario::ScenarioDocumentModel>>();

  actions.add<Actions::RemoveElements>(m_removeElements);
  actions.add<Actions::CopyContent>(m_copyContent);
  actions.add<Actions::CutContent>(m_cutContent);
  actions.add<Actions::PasteElements>(m_pasteElements);
  actions.add<Actions::PasteElementsAfter>(m_pasteElementsAfter);
  actions.add<Actions::ElementsToJson>(m_elementsToJson);
  actions.add<Actions::MergeTimeSyncs>(m_mergeTimeSyncs);
  actions.add<Actions::MergeEvents>(m_mergeEvents);
  actions.add<Actions::Encapsulate>(m_encapsulate);
  actions.add<Actions::Decapsulate>(m_decapsulate);
  actions.add<Actions::Duplicate>(m_duplicate);

  scenariomodel_cond.add<Actions::MergeTimeSyncs>();
  scenariomodel_cond.add<Actions::Encapsulate>();
  scenariomodel_cond.add<Actions::Decapsulate>();
  //scenariofocus_cond.add<Actions::PasteElements>();

  //scenarioiface_cond.add<Actions::CopyContent>();
  //scenarioiface_cond.add<Actions::CutContent>();
  scenarioiface_cond.add<Actions::ElementsToJson>();

  Menu& object = base_menus.at(Menus::Object());
  object.menu()->addAction(m_elementsToJson);
  object.menu()->addAction(m_removeElements);
  object.menu()->addSeparator();
  object.menu()->addAction(m_copyContent);
  object.menu()->addAction(m_cutContent);
  object.menu()->addAction(m_pasteElements);
  object.menu()->addAction(m_pasteElementsAfter);
  object.menu()->addSeparator();
  object.menu()->addAction(m_mergeTimeSyncs);
  object.menu()->addAction(m_mergeEvents);
  object.menu()->addAction(m_encapsulate);
  object.menu()->addAction(m_decapsulate);
  object.menu()->addAction(m_duplicate);
  m_eventActions.makeGUIElements(e);
  m_cstrActions.makeGUIElements(e);

  Menu& view = base_menus.at(Menus::View());
  view.menu()->addAction(m_selectAll);
  view.menu()->addAction(m_deselectAll);
  view.menu()->addAction(m_selectTop);
  view.menu()->addAction(m_goToParent);

  e.actions.add<Actions::SelectAll>(m_selectAll);
  e.actions.add<Actions::DeselectAll>(m_deselectAll);
  e.actions.add<Actions::SelectTop>(m_selectTop);
  e.actions.add<Actions::GoToParent>(m_goToParent);

  scenariodocument_cond.add<Actions::SelectAll>();
  scenariodocument_cond.add<Actions::DeselectAll>();
  scenariodocument_cond.add<Actions::SelectTop>();
  scenariodocument_cond.add<Actions::GoToParent>();
}

void ObjectMenuActions::setupContextMenu(Process::LayerContextMenuManager& ctxm)
{
  using namespace Process;
  LayerContextMenu scenario_model
      = MetaContextMenu<ContextMenus::ScenarioModelContextMenu>::make();
  LayerContextMenu scenario_object
      = MetaContextMenu<ContextMenus::ScenarioObjectContextMenu>::make();

  // Used for scenario model
  scenario_model.functions.push_back(
      [this](QMenu& menu, QPoint, QPointF scenePoint, const LayerContext& ctx) {
    auto scenario = qobject_cast<const ScenarioPresenter*>(&ctx.presenter);
    if(!scenario)
      return;

    auto views = scenario->view().scene()->views();
    if(views.empty())
      return;

    auto view = views.front();
    auto pt = view->mapToGlobal(view->mapFromScene(scenePoint));

    auto objectMenu = menu.addMenu(tr("Object"));

    objectMenu->addAction(m_elementsToJson);
    objectMenu->addAction(m_removeElements);
    objectMenu->addSeparator();

    objectMenu->addAction(m_copyContent);
    objectMenu->addAction(m_cutContent);

    // We use a different Paste action here because we need to use the mouse cursor
    // at the time of the creation of the menu, not at the time of clicking on "Paste".
    auto pasteElements = new QAction{tr("Paste element(s)"), this};
    pasteElements->setShortcut(QKeySequence::Paste);
    pasteElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(pasteElements, &QAction::triggered, this, [this, pt] {
      this->pasteElements(pt);
    });
    menu.addAction(pasteElements);

    objectMenu->addAction(m_pasteElementsAfter);
    objectMenu->addAction(m_duplicate);
    objectMenu->addSeparator();
    objectMenu->addAction(m_encapsulate);
    objectMenu->addAction(m_decapsulate);
  });

  // Used for base scenario, loops, etc.
  scenario_object.functions.push_back(
      [this](QMenu& menu, QPoint, QPointF, const LayerContext& ctx) {
    auto objectMenu = menu.addMenu(tr("Object"));

    objectMenu->addAction(m_elementsToJson);
    objectMenu->addSeparator();

    objectMenu->addAction(m_copyContent);
  });

  m_eventActions.setupContextMenu(ctxm);
  m_cstrActions.setupContextMenu(ctxm);

  ctxm.insert(std::move(scenario_model));
  ctxm.insert(std::move(scenario_object));
}

void ObjectMenuActions::copySelectedElementsToJson(JSONReader& r)
{
  auto doc = m_parent->currentDocument();
  if(!doc)
    return;

  const auto& ctx = doc->context();
  const auto& cur_sel = ctx.selectionStack.currentSelection();

  auto& rm = ctx.app.interfaces<score::ObjectEditorList>();
  for(auto& iface : rm)
  {
    if(iface.copy(r, cur_sel, ctx))
      break;
  }
}

void ObjectMenuActions::removeSelectedElements()
{
  auto doc = m_parent->currentDocument();
  if(!doc)
    return;

  auto& ctx = doc->context();
  const auto& cur_sel = ctx.selectionStack.currentSelection();

  auto& rm = ctx.app.interfaces<score::ObjectEditorList>();
  for(auto& iface : rm)
  {
    if(iface.remove(cur_sel, ctx))
      break;
  }
}

void ObjectMenuActions::cutSelectedElementsToJson(JSONReader& r)
{
  auto doc = m_parent->currentDocument();
  if(!doc)
    return;

  auto& ctx = doc->context();
  const auto& cur_sel = ctx.selectionStack.currentSelection();

  auto& rm = ctx.app.interfaces<score::ObjectEditorList>();
  for(auto& iface : rm)
  {
    if(iface.copy(r, cur_sel, ctx))
    {
      iface.remove(cur_sel, ctx);
      break;
    }
  }
}

void ObjectMenuActions::pasteElements(QPoint pos)
{
  auto doc = m_parent->currentDocument();
  if(!doc)
    return;

  auto& ctx = doc->context();
  Process::LayerPresenter* layer{};
  {
    if(auto focus = Process::ProcessFocusManager::get(ctx))
    {
      layer = qobject_cast<Process::LayerPresenter*>(focus->focusedPresenter());
    }
  }

  auto& rm = ctx.app.interfaces<score::ObjectEditorList>();
  const auto& md = *qApp->clipboard()->mimeData();
  for(auto& iface : rm)
  {
    if(iface.paste(pos, layer, md, ctx))
      break;
  }
}

void ObjectMenuActions::pasteElements()
{
  if(!isFocusingScenario())
    return;
  pasteElements(QCursor::pos());
}

void ObjectMenuActions::pasteElementsAfter(const rapidjson::Value& obj)
{
  if(!obj.IsObject() || obj.MemberCount() == 0)
    return;
  if(!obj.HasMember("TimeNodes"))
    return;

  // TODO check for unnecessary uses of focusedProcessModel after
  // focusedPresenter.
  auto pres = m_parent->focusedPresenter();
  if(!pres)
    return;

  auto sp = dynamic_cast<ScenarioPresenter*>(pres);
  if(!sp)
    return;

  // TODO check json validity
  if(auto ts = furthestHierarchicallySelectedTimeSync(*sp))
  {
    // TODO is there a way to compute the actual scale ?
    auto cmd = new Command::ScenarioPasteElementsAfter{sp->model(), *ts, obj, 1.0};
    dispatcher().submit(cmd);
  }
}

bool ObjectMenuActions::isFocusingScenario() const noexcept
{
  auto doc = m_parent->context.documents.currentDocument();
  if(!doc)
    return false;

  auto widg = qApp->focusWidget();
  if(!widg)
    return false;

  auto delegate = qobject_cast<ScenarioDocumentView*>(&doc->view()->viewDelegate());
  if(!delegate)
    return false;

  if(widg != &delegate->view())
    return false;

  return true;
}

ScenarioDocumentModel* ObjectMenuActions::getScenarioDocModel() const
{
  if(auto doc = m_parent->currentDocument())
    return score::IDocument::try_get<ScenarioDocumentModel>(*doc);
  return nullptr;
}

ScenarioDocumentPresenter* ObjectMenuActions::getScenarioDocPresenter() const
{
  if(auto doc = m_parent->currentDocument())
    return score::IDocument::try_get<ScenarioDocumentPresenter>(*doc);
  return nullptr;
}

CommandDispatcher<> ObjectMenuActions::dispatcher() const
{
  CommandDispatcher<> disp{m_parent->currentDocument()->context().commandStack};
  return disp;
}
}
