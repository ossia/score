// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioApplicationPlugin.hpp"

#include "Menus/TransportActions.hpp"

#include <Inspector/InspectorWidgetList.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/Process.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Application/Drops/AutomationDropHandler.hpp>
#include <Scenario/Application/Menus/ObjectMenuActions.hpp>
#include <Scenario/Application/Menus/ScenarioContextMenuManager.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsPresenter.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/LayerData.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Palette/Tool.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>

#include <score/actions/Menu.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentView.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/Window.hpp>

#include <QAction>
#include <QMenu>
#include <qnamespace.h>

#include <string.h>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Scenario::ScenarioExecution)
W_OBJECT_IMPL(Scenario::ScenarioApplicationPlugin)
namespace Scenario
{
ScenarioExecution::ScenarioExecution() { }
ScenarioExecution::~ScenarioExecution() { }
void test_parse_expr_full();

ScenarioApplicationPlugin::ScenarioApplicationPlugin(const score::GUIApplicationContext& ctx)
    : GUIApplicationPlugin{ctx}
{
  auto app = QCoreApplication::instance();
  if (auto guiapp = qobject_cast<QGuiApplication*>(app))
  {
    connect(guiapp, &QGuiApplication::applicationStateChanged, this, [&](Qt::ApplicationState st) {
      editionSettings().setDefault();
    });
  }

  // Register conditions for the actions enablement
  using namespace score;
  using namespace Process;
  ctx.actions.onFocusChange(std::make_shared<EnableWhenFocusedProcessIs<ProcessModel>>());
  ctx.actions.onFocusChange(std::make_shared<EnableWhenFocusedProcessIs<ScenarioInterface>>());
  ctx.actions.onDocumentChange(std::make_shared<EnableWhenDocumentIs<ScenarioDocumentModel>>());

  ctx.actions.onSelectionChange(std::make_shared<EnableWhenSelectionContains<IntervalModel>>());
  ctx.actions.onSelectionChange(std::make_shared<EnableWhenSelectionContains<EventModel>>());
  ctx.actions.onSelectionChange(std::make_shared<EnableWhenSelectionContains<StateModel>>());
  ctx.actions.onSelectionChange(std::make_shared<EnableWhenSelectionContains<TimeSyncModel>>());

  auto on_sm = std::make_shared<EnableWhenScenarioModelObject>();
  ctx.actions.onSelectionChange(on_sm);
  ctx.actions.onFocusChange(on_sm);
  auto on_instant_si = std::make_shared<EnableWhenScenarioInterfaceInstantObject>();
  ctx.actions.onSelectionChange(on_instant_si);
  ctx.actions.onFocusChange(on_instant_si);
  auto on_si = std::make_shared<EnableWhenScenarioInterfaceObject>();
  ctx.actions.onSelectionChange(on_si);
  ctx.actions.onFocusChange(on_si);

  if (context.applicationSettings.gui)
  {
    m_objectActions.setupContextMenu(m_layerCtxMenuManager);
  }

  {
    // Dataflow
    m_showCables = new QAction{this};
    m_showCables->setCheckable(true);
    m_showCables->setChecked(true);
    setIcons(
        m_showCables,
        QStringLiteral(":/icons/show_cables_on.png"),
        QStringLiteral(":/icons/show_cables_hover.png"),
        QStringLiteral(":/icons/show_cables_off.png"),
        QStringLiteral(":/icons/show_cables_disabled.png"));
    connect(m_showCables, &QAction::toggled, this, [this](bool c) {
      auto doc = this->currentDocument();
      if (doc)
      {
        Dataflow::CableItem::g_cables_enabled = c;

        ScenarioDocumentPresenter* plug
            = score::IDocument::try_get<ScenarioDocumentPresenter>(*doc);
        if (plug)
        {
          for (const auto& port : plug->context().dataflow.ports())
          {
            Dataflow::PortItem& item = *port.second;
            item.resetPortVisible();
          }

          for (auto& cable : plug->context().dataflow.cables())
          {
            cable.second->check();
          }
        }
      }
    });
  }

  {
    // Fold / unfold intervals
    m_foldIntervals = new QAction{this};
    connect(m_foldIntervals, &QAction::triggered, this, [this] {
      auto doc = this->currentDocument();
      if (!doc)
        return;
      auto scenario = focusedScenarioInterface(doc->context());
      if (!scenario)
        return;
      for (IntervalModel& itv : scenario->getIntervals())
      {
        itv.setSmallViewVisible(false);
      }
    });
    m_unfoldIntervals = new QAction{this};
    connect(m_unfoldIntervals, &QAction::triggered, this, [this] {
      auto doc = this->currentDocument();
      if (!doc)
        return;
      auto scenario = focusedScenarioInterface(doc->context());
      if (!scenario)
        return;
      for (IntervalModel& itv : scenario->getIntervals())
      {
        itv.setSmallViewVisible(true);
      }
    });
  }

  {
    m_levelUp = new QAction{this};
    connect(m_levelUp, &QAction::triggered, this, [this] {
      auto doc = this->currentDocument();
      if (!doc)
        return;

      ScenarioDocumentPresenter* pres
          = IDocument::presenterDelegate<ScenarioDocumentPresenter>(*doc);
      if (!pres)
        return;

      const auto cst_pres = pres->presenters().intervalPresenter();
      if (cst_pres)
      {
        const QObject* itv = &cst_pres->model();
        while (itv)
        {
          itv = itv->parent();
          if (auto itv_ = qobject_cast<const IntervalModel*>(itv))
          {
            pres->setDisplayedInterval(const_cast<IntervalModel&>(*itv_));
            return;
          }
        }
      }
    });
  }
}

void ScenarioApplicationPlugin::initialize()
{
  // Needs a delayed init because it scans all the registered factories
  {
    auto& droppers = context.interfaces<Scenario::DropHandlerList>();
    auto dropper
        = (DropProcessInScenario*)droppers.get(DropProcessInScenario::static_concreteKey());
    SCORE_ASSERT(dropper);
    dropper->init();
  }

  // Add a default factory for process inspectors, after everything else is
  // done
  {
    auto& pw = const_cast<Inspector::InspectorWidgetList&>(
        context.interfaces<Inspector::InspectorWidgetList>());
    pw.insert(std::make_unique<Process::DefaultInspectorWidgetDelegateFactory>());
  }
}

auto ScenarioApplicationPlugin::makeGUIElements() -> GUIElements
{
  using namespace score;
  GUIElements e;

  m_objectActions.makeGUIElements(e);
  m_toolActions.makeGUIElements(e);
  m_transportActions.makeGUIElements(e);

  // Dataflow
  auto& actions = e.actions;
  actions.add<Actions::ShowCables>(m_showCables);
  actions.add<Actions::FoldIntervals>(m_foldIntervals);
  actions.add<Actions::UnfoldIntervals>(m_unfoldIntervals);
  actions.add<Actions::LevelUp>(m_levelUp);

  score::Menu& menu = context.menus.get().at(score::Menus::View());
  menu.menu()->addAction(m_showCables);
  menu.menu()->addAction(m_foldIntervals);
  menu.menu()->addAction(m_unfoldIntervals);
  menu.menu()->addAction(m_levelUp);

  return e;
}

ScenarioApplicationPlugin::~ScenarioApplicationPlugin() = default;

void ScenarioApplicationPlugin::on_presenterDefocused(Process::LayerPresenter* pres)
{
  // We set the currently focused view model to a "select" state
  // to prevent problems.
  editionSettings().setDefault();

  disconnect(m_contextMenuConnection);
}

void ScenarioApplicationPlugin::on_presenterFocused(Process::LayerPresenter* pres)
{
  // Generic stuff
  disconnect(m_contextMenuConnection);
  disconnect(m_keyPressConnection);
  disconnect(m_keyReleaseConnection);
  if (pres)
  {
    // If a layer is right-clicked,
    // this is called and will create a context menu with slot & process
    // information.
    m_contextMenuConnection = QObject::connect(
        pres,
        &Process::LayerPresenter::contextMenuRequested,
        this,
        [=](const QPoint& pos, const QPointF& pt2) {
          QMenu menu(qApp->activeWindow());
          pres->fillContextMenu(menu, pos, pt2, m_layerCtxMenuManager);
          menu.exec(pos);
          menu.close();
        });
  }
  else
  {
    return;
  }

  auto s_pres = dynamic_cast<ScenarioPresenter*>(pres);
  if (s_pres)
  {
    m_keyPressConnection = connect(s_pres, &ScenarioPresenter::keyPressed,
                                   this, &ScenarioApplicationPlugin::keyPressed);

    m_keyReleaseConnection = connect(s_pres, &ScenarioPresenter::keyReleased,
                                     this, &ScenarioApplicationPlugin::keyReleased);

    auto& select_act = context.actions.action<Actions::SelectTool>();
    select_act.action()->trigger();
  }
}

void ScenarioApplicationPlugin::on_documentChanged(
    score::Document* olddoc,
    score::Document* newdoc)
{
  using namespace score;
  // TODO the context menu connection should be reviewed, too.
  this->disconnect(m_focusConnection);
  this->disconnect(m_defocusConnection);

  m_editionSettings.setDefault();
  m_editionSettings.setExecution(false);

  if (!newdoc)
    return;

  // Load cables
  auto& model = score::IDocument::modelDelegate<Scenario::ScenarioDocumentModel>(*newdoc);
  model.finishLoading();
  // TODO do htis on restore

  // Setup ui
  if (!newdoc->context().app.mainWindow)
    return;

  auto focusManager = processFocusManager();

  if (!focusManager)
    return;

  m_focusConnection = connect(
      focusManager,
      &Process::ProcessFocusManager::sig_focusedPresenter,
      this,
      &ScenarioApplicationPlugin::on_presenterFocused);
  m_defocusConnection = connect(
      focusManager,
      &Process::ProcessFocusManager::sig_defocusedPresenter,
      this,
      &ScenarioApplicationPlugin::on_presenterDefocused);

  if (focusManager->focusedPresenter())
  {
    // Used when switching between documents
    on_presenterFocused(focusManager->focusedPresenter());
  }
  else
  {
    // We focus by default the first process of the interval in full view
    // we're in
    // TODO this snippet is useful, put it somewhere in some Toolkit file.
    ScenarioDocumentPresenter* pres
        = IDocument::presenterDelegate<ScenarioDocumentPresenter>(*newdoc);
    if (pres)
    {
      FullViewIntervalPresenter* cst_pres = pres->presenters().intervalPresenter();

      if (cst_pres && !cst_pres->getSlots().empty())
      {
        auto& firstSlot = cst_pres->getSlots().front();
        if(auto slt = firstSlot.getLayerSlot())
        {
          auto p = slt->layers.front().mainPresenter();
          if (p)
            focusManager->focus(p);
        }
      }
    }
  }

  // Finally we focus the View widget.
  if (auto v = newdoc->view())
  {
    auto bev = dynamic_cast<ScenarioDocumentView*>(&v->viewDelegate());
    if (bev)
      bev->view().setFocus();
  }
}

void ScenarioApplicationPlugin::on_activeWindowChanged()
{
  editionSettings().setDefault(); // NOTE maybe useless now ?
}

ScenarioPresenter* ScenarioApplicationPlugin::focusedPresenter() const
{
  if (auto focusManager = processFocusManager())
  {
    return dynamic_cast<ScenarioPresenter*>(focusManager->focusedPresenter());
  }
  return nullptr;
}

void ScenarioApplicationPlugin::on_initDocument(score::Document& doc) { }

void ScenarioApplicationPlugin::on_createdDocument(score::Document& doc) { }

void ScenarioApplicationPlugin::prepareNewDocument()
{
  if (context.applicationSettings.gui)
  {
    auto& stop_action = context.actions.action<Actions::Stop>();
    stop_action.action()->trigger();
  }
}

Process::ProcessFocusManager* ScenarioApplicationPlugin::processFocusManager() const
{
  if (auto doc = currentDocument())
  {
    if (auto pres = doc->presenter())
    {
      auto bem = dynamic_cast<ScenarioDocumentPresenter*>(pres->presenterDelegate());
      if (bem)
      {
        return &bem->focusManager();
      }
    }
  }

  return nullptr;
}
}
