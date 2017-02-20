#include <QAction>
#include <QByteArray>
#include <QClipboard>
#include <QQuickWidget>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QObject>
#include <iscore/document/DocumentInterface.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Constraint/InsertContentInConstraint.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteContent.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElements.hpp>
#include <Scenario/Commands/State/InsertContentInState.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioGlobalCommandManager.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/actions/MenuManager.hpp>
#include <qnamespace.h>

#include <QRect>
#include <QString>
#include <QToolBar>
#include <algorithm>

#include "ObjectMenuActions.hpp"
#include <Process/LayerModel.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>

#include "ScenarioCopy.hpp"
#include "TextDialog.hpp"
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <core/document/Document.hpp>
#include <iscore/application/ApplicationContext.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Commands/Cohesion/DoForSelectedConstraints.hpp>
#include <Scenario/Process/Algorithms/ContainersAccessors.hpp>
#include <iscore/actions/ActionManager.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/actions/Menu.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
ObjectMenuActions::ObjectMenuActions(ScenarioApplicationPlugin* parent)
    : m_parent{parent}
    , m_eventActions{parent}
    , m_cstrActions{parent}
    , m_stateActions{parent}
{
  using namespace iscore;

  // REMOVE
  m_removeElements = new QAction{tr("Remove selected elements"), this};
  m_removeElements->setShortcut(Qt::Key_Backspace); // NOTE : the effective
                                                    // shortcut is in
                                                    // CommonSelectionState.cpp
  m_removeElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  connect(m_removeElements, &QAction::triggered, [this]() {
    auto& ctx = m_parent->currentDocument()->context();

    auto sm = focusedScenarioModel(ctx);
    if (sm)
    {
      Scenario::removeSelection(*sm, ctx.commandStack);
    }
  });

  m_clearElements = new QAction{tr("Clear selected elements"), this};
  m_clearElements->setShortcut(
      QKeySequence::Delete); // NOTE : the effective shortcut is in
                             // CommonSelectionState.cpp
  m_clearElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  connect(m_clearElements, &QAction::triggered, [this]() {
    auto& ctx = m_parent->currentDocument()->context();

    if (auto sm = focusedScenarioModel(ctx))
    {
      Scenario::clearContentFromSelection(*sm, ctx.commandStack);
    }
    else if (auto si = focusedScenarioInterface(ctx))
    {
      Scenario::clearContentFromSelection(*si, ctx.commandStack);
    }
  });

  // COPY/CUT
  m_copyContent = new QAction{tr("Copy"), this};
  m_copyContent->setShortcut(QKeySequence::Copy);
  m_copyContent->setShortcutContext(Qt::ApplicationShortcut);
  connect(m_copyContent, &QAction::triggered, [this]() {
    auto obj = copySelectedElementsToJson();
    if (obj.empty())
      return;
    QJsonDocument doc{obj};
    auto clippy = QApplication::clipboard();
    clippy->setText(doc.toJson(QJsonDocument::Indented));
  });

  m_cutContent = new QAction{tr("Cut"), this};
  m_cutContent->setShortcut(QKeySequence::Cut);
  m_cutContent->setShortcutContext(Qt::ApplicationShortcut);
  connect(m_cutContent, &QAction::triggered, [this]() {
    auto obj = cutSelectedElementsToJson();
    if (obj.empty())
      return;
    QJsonDocument doc{obj};
    auto clippy = QApplication::clipboard();
    clippy->setText(doc.toJson(QJsonDocument::Indented));
  });

  m_pasteContent = new QAction{tr("Paste content"), this};
  m_pasteContent->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  connect(m_pasteContent, &QAction::triggered, [this]() {
    writeJsonToSelectedElements(
        QJsonDocument::fromJson(QApplication::clipboard()->text().toUtf8())
            .object());
  });

  m_pasteElements = new QAction{tr("Paste elements"), this};
  m_pasteElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  connect(m_pasteElements, &QAction::triggered, [this]() {
    auto pres = m_parent->focusedPresenter();
    if (!pres)
      return;
    auto views = pres->view().scene()->views();
    if (views.empty())
      return;

    auto view = views.front();

    QPoint pos = QCursor::pos();

    auto scene_pt = view->mapToScene(view->mapFromGlobal(pos));
    TemporalScenarioView& sv = pres->view();
    auto sv_pt = sv.mapFromScene(scene_pt);
    if (!sv.contains(sv_pt))
    {
      sv_pt = sv.mapToScene(sv.boundingRect().center());
    }
    auto pt = pres->toScenarioPoint(sv_pt);
    pasteElements(
        QJsonDocument::fromJson(QApplication::clipboard()->text().toUtf8())
            .object(),
        pt);
  });

  // DISPLAY JSON
  m_elementsToJson = new QAction{tr("Convert selection to JSON"), this};
  connect(m_elementsToJson, &QAction::triggered, [this]() {
    QJsonDocument doc{copySelectedElementsToJson()};
    TextDialog s{doc.toJson(QJsonDocument::Indented), qApp->activeWindow()};

    s.exec();
  });

  // MERGE TIMENODES
  m_mergeTimeNodes = new QAction{this};
  connect(m_mergeTimeNodes, &QAction::triggered, [this]() {
    auto sm = focusedScenarioModel(m_parent->currentDocument()->context());
    ISCORE_ASSERT(sm);

    Scenario::mergeTimeNodes(
        *sm, m_parent->currentDocument()->context().commandStack);
  });

  parent->context.mainWindow.addAction(m_removeElements);
  parent->context.mainWindow.addAction(m_clearElements);


  // Selection actions
  m_selectAll = new QAction{tr("Select all"), this};
  m_selectAll->setToolTip("Ctrl+A");
  connect(m_selectAll, &QAction::triggered, [this]() {
    if (auto pres = getScenarioDocPresenter())
      pres->selectAll();
  });

  m_deselectAll = new QAction{tr("Deselect all"), this};
  m_deselectAll->setToolTip("Ctrl+Shift+A");
  connect(m_deselectAll, &QAction::triggered, [this]() {
    if (auto pres = getScenarioDocPresenter())
      pres->deselectAll();
  });

  m_selectTop = new QAction{this};
  connect(m_selectTop, &QAction::triggered, [this] {
    if (auto pres = getScenarioDocPresenter())
      pres->selectTop();
  });
}

void ObjectMenuActions::makeGUIElements(iscore::GUIElements& e)
{
  using namespace iscore;
  auto& actions = e.actions;
  auto& base_menus = m_parent->context.menus.get();

  auto& scenariofocus_cond
      = m_parent->context.actions
            .condition<Process::
                           EnableWhenFocusedProcessIs<Scenario::
                                                          ProcessModel>>();
  auto& scenariomodel_cond
      = m_parent->context.actions.condition<EnableWhenScenarioModelObject>();
  auto& scenarioiface_cond
      = m_parent->context.actions
            .condition<EnableWhenScenarioInterfaceObject>();

  auto& scenariodocument_cond
      = m_parent->context.actions
            .condition<iscore::
                           EnableWhenDocumentIs<Scenario::
                                                    ScenarioDocumentModel>>();

  actions.add<Actions::RemoveElements>(m_removeElements);
  actions.add<Actions::ClearElements>(m_clearElements);
  actions.add<Actions::CopyContent>(m_copyContent);
  actions.add<Actions::CutContent>(m_cutContent);
  actions.add<Actions::PasteContent>(m_pasteContent);
  actions.add<Actions::PasteElements>(m_pasteElements);
  actions.add<Actions::ElementsToJson>(m_elementsToJson);
  actions.add<Actions::MergeTimeNodes>(m_mergeTimeNodes);

  scenariomodel_cond.add<Actions::RemoveElements>();
  scenariomodel_cond.add<Actions::MergeTimeNodes>();
  scenariofocus_cond.add<Actions::PasteElements>();

  scenarioiface_cond.add<Actions::ClearElements>();
  scenarioiface_cond.add<Actions::CopyContent>();
  scenarioiface_cond.add<Actions::CutContent>();
  scenarioiface_cond.add<Actions::PasteContent>();
  scenarioiface_cond.add<Actions::ElementsToJson>();

  Menu& object = base_menus.at(Menus::Object());
  object.menu()->addAction(m_elementsToJson);
  object.menu()->addAction(m_removeElements);
  object.menu()->addAction(m_clearElements);
  object.menu()->addSeparator();
  object.menu()->addAction(m_copyContent);
  object.menu()->addAction(m_cutContent);
  object.menu()->addAction(m_pasteContent);
  object.menu()->addAction(m_pasteElements);
  object.menu()->addSeparator();
  object.menu()->addAction(m_mergeTimeNodes);
  m_eventActions.makeGUIElements(e);
  m_cstrActions.makeGUIElements(e);
  m_stateActions.makeGUIElements(e);

  Menu& view = base_menus.at(Menus::View());
  view.menu()->addAction(m_selectAll);
  view.menu()->addAction(m_deselectAll);
  view.menu()->addAction(m_selectTop);

  e.actions.add<Actions::SelectAll>(m_selectAll);
  e.actions.add<Actions::DeselectAll>(m_deselectAll);
  e.actions.add<Actions::SelectTop>(m_selectTop);

  scenariodocument_cond.add<Actions::SelectAll>();
  scenariodocument_cond.add<Actions::DeselectAll>();
  scenariodocument_cond.add<Actions::SelectTop>();

}

void ObjectMenuActions::setupContextMenu(
    Process::LayerContextMenuManager& ctxm)
{
  using namespace Process;
  LayerContextMenu scenario_model
      = MetaContextMenu<ContextMenus::ScenarioModelContextMenu>::make();
  LayerContextMenu scenario_object
      = MetaContextMenu<ContextMenus::ScenarioObjectContextMenu>::make();

  // Used for scenario model
  scenario_model.functions.push_back([this](
      QMenu& menu, QPoint, QPointF scenePoint, const LayerContext& ctx) {
    auto& scenario
        = *safe_cast<const TemporalScenarioPresenter*>(&ctx.presenter);
    auto sel = ctx.context.selectionStack.currentSelection();
    if (!sel.empty())
    {
      auto objectMenu = menu.addMenu(tr("Object"));

      objectMenu->addAction(m_elementsToJson);
      objectMenu->addAction(m_removeElements);
      objectMenu->addAction(m_clearElements);
      objectMenu->addSeparator();

      objectMenu->addAction(m_copyContent);
      objectMenu->addAction(m_cutContent);
      objectMenu->addAction(m_pasteContent);
    }

    auto pasteElements = new QAction{tr("Paste element(s)"), this};
    pasteElements->setShortcut(QKeySequence::Paste);
    pasteElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(pasteElements, &QAction::triggered, [&, scenePoint]() {
      this->pasteElements(
          QJsonDocument::fromJson(QApplication::clipboard()->text().toUtf8())
              .object(),
          scenario.toScenarioPoint(scenario.view().mapFromScene(scenePoint)));
    });
    menu.addAction(pasteElements);
  });

  // Used for base scenario, loops, etc.
  scenario_object.functions.push_back(
      [this](QMenu& menu, QPoint, QPointF, const LayerContext& ctx) {
        auto sel = ctx.context.selectionStack.currentSelection();
        if (!sel.empty())
        {
          auto objectMenu = menu.addMenu(tr("Object"));

          objectMenu->addAction(m_elementsToJson);
          objectMenu->addAction(m_clearElements);
          objectMenu->addSeparator();

          objectMenu->addAction(m_copyContent);
          objectMenu->addAction(m_pasteContent);
        }
      });
  m_eventActions.setupContextMenu(ctxm);
  m_cstrActions.setupContextMenu(ctxm);
  m_stateActions.setupContextMenu(ctxm);

  ctxm.insert(std::move(scenario_model));
  ctxm.insert(std::move(scenario_object));
}

QJsonObject ObjectMenuActions::copySelectedElementsToJson()
{
  auto si = focusedScenarioInterface(m_parent->currentDocument()->context());
  auto si_obj = dynamic_cast<QObject*>(const_cast<ScenarioInterface*>(si));
  if (auto sm = dynamic_cast<const Scenario::ProcessModel*>(si))
  {
    return copySelectedScenarioElements(*sm);
  }
  else if (auto bsm = dynamic_cast<const Scenario::BaseScenarioContainer*>(si))
  {
    return copySelectedScenarioElements(*bsm, si_obj);
  }
  else
  {
    // Full-view copy
    auto& bem
        = iscore::IDocument::modelDelegate<Scenario::ScenarioDocumentModel>(
            *m_parent->currentDocument());
    if (!bem.baseScenario().selectedChildren().empty())
    {
      return copySelectedScenarioElements(
          bem.baseScenario(), &bem.baseScenario());
    }
  }

  return {};
}

QJsonObject ObjectMenuActions::cutSelectedElementsToJson()
{
  auto obj = copySelectedElementsToJson();
  if (obj.empty())
    return {};

  auto& ctx = m_parent->currentDocument()->context();

  if (auto sm = focusedScenarioModel(ctx))
  {
    Scenario::removeSelection(*sm, ctx.commandStack);
  }
  else if (auto si = focusedScenarioInterface(ctx))
  {
    Scenario::clearContentFromSelection(*si, ctx.commandStack);
  }

  return obj;
}

void ObjectMenuActions::pasteElements(
    const QJsonObject& obj, const Scenario::Point& origin)
{
  // TODO check for unnecessary uses of focusedProcessModel after
  // focusedPresenter.
  auto pres = m_parent->focusedPresenter();
  if (!pres)
    return;

  auto& sm = static_cast<const TemporalScenarioLayer&>(pres->layerModel());
  // TODO check json validity
  auto cmd = new Command::ScenarioPasteElements(sm, obj, origin);

  dispatcher().submitCommand(cmd);
}

template <typename Scenario_T>
static void writeJsonToScenario(
    const Scenario_T& scen,
    const ObjectMenuActions& self,
    const QJsonObject& obj)
{
  MacroCommandDispatcher<Command::ScenarioPasteContent> dispatcher{
      self.dispatcher().stack()};
  auto selectedConstraints = selectedElements(getConstraints(scen));
  auto expandMode = self.appPlugin()->editionSettings().expandMode();
  for (const auto& json_vref : obj["Constraints"].toArray())
  {
    for (const auto& constraint : selectedConstraints)
    {
      auto cmd = new Scenario::Command::InsertContentInConstraint{
          json_vref.toObject(), *constraint, expandMode};

      dispatcher.submitCommand(cmd);
    }
  }

  auto selectedStates = selectedElements(getStates(scen));
  for (const auto& json_vref : obj["States"].toArray())
  {
    for (const auto& state : selectedStates)
    {
      auto cmd
          = new Command::InsertContentInState{json_vref.toObject(), *state};

      dispatcher.submitCommand(cmd);
    }
  }

  dispatcher.commit();
}

void ObjectMenuActions::writeJsonToSelectedElements(const QJsonObject& obj)
{
  auto si = focusedScenarioInterface(m_parent->currentDocument()->context());
  if (auto sm = dynamic_cast<const Scenario::ProcessModel*>(si))
  {
    writeJsonToScenario(*sm, *this, obj);
  }
  else if (auto bsm = dynamic_cast<const Scenario::BaseScenarioContainer*>(si))
  {
    writeJsonToScenario(*bsm, *this, obj);
  }
  else
  {
    ISCORE_TODO;
    /*
    // Full-view paste
    auto& bem =
    iscore::IDocument::modelDelegate<ScenarioDocumentModel>(*m_parent->currentDocument());
    if(bem.baseConstraint().selection.get())
    {
        return copySelectedScenarioElements(bem.baseScenario());
    }*/
  }
}

ScenarioDocumentModel*ObjectMenuActions::getScenarioDocModel() const
{
  if(auto doc = m_parent->currentDocument())
    return iscore::IDocument::try_get<ScenarioDocumentModel>(*doc);
  return nullptr;
}

ScenarioDocumentPresenter*ObjectMenuActions::getScenarioDocPresenter() const
{
  if(auto doc = m_parent->currentDocument())
    return iscore::IDocument::try_get<ScenarioDocumentPresenter>(*doc);
  return nullptr;
}

CommandDispatcher<> ObjectMenuActions::dispatcher() const
{
  CommandDispatcher<> disp{
      m_parent->currentDocument()->context().commandStack};
  return disp;
}
}
