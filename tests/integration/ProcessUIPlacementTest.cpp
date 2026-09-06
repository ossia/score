// Integration tests: where the script editors and custom UIs of processes
// open (separate window, right pane, central view), how they move between
// those places, every way they close, what the document remembers, and the
// chromeless editor drawn over the document's background.
//
// Everything runs through the same entry points the UI uses
// (Process::setupScriptUI & co), with the main window hidden.
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Script/EditorOverlay.hpp>
#include <Process/UIPlacement.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/AddressBarWidget.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <Effect/EffectLayer.hpp>
#include <JS/JSProcessModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/graphics/BackgroundRenderer.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentView.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/view/CentralViewStack.hpp>
#include <core/view/FixedTabWidget.hpp>
#include <core/view/Window.hpp>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QSettings>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Keyboard.hpp>
#include <score_test/Mouse.hpp>

#include <cmath>

namespace
{
using Process::UIPlacement;
using Process::UIPlacementSettings;

//! Deferred deletions, timers and layout passes: let them run. Without an
//! event loop running, deleteLater() only takes effect when asked for.
void spin(int ms = 50)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QApplication::processEvents(QEventLoop::AllEvents, 5);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  } while(t.elapsed() < ms);
}

Scenario::IntervalModel& baseInterval(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
      .baseInterval();
}

const UuidKey<Process::ProcessModel> jsKey = UuidKey<Process::ProcessModel>::fromString(
    QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0"));

//! A Javascript process: a script editor with two tabs, a dockable UI.
Process::ProcessModel& addJs(score::Document& doc)
{
  auto& ctx = doc.context();
  auto& itv = baseInterval(doc);
  auto* factory = ctx.app.interfaces<Process::ProcessFactoryList>().get(jsKey);
  REQUIRE(factory != nullptr);
  CommandDispatcher<> disp{ctx.commandStack};
  disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
      itv, factory->concreteKey(), factory->customConstructionData(), QPointF{});
  Process::ProcessModel* js{};
  for(auto& p : itv.processes)
    if(p.concreteKey() == jsKey)
      js = &p;
  REQUIRE(js != nullptr);
  return *js;
}

score::View& mainView(const score::GUIApplicationContext& ctx)
{
  auto v = qobject_cast<score::View*>(ctx.mainWindow);
  REQUIRE(v != nullptr);
  return *v;
}

score::CentralViewStack& centralViews(score::Document& doc)
{
  REQUIRE(doc.view() != nullptr);
  return doc.view()->centralViews();
}

int rightTabCount(const score::GUIApplicationContext& ctx)
{
  return mainView(ctx).rightTabs->actionGroup()->actions().size();
}

void setDefault(const char* key, const QString& value)
{
  QSettings{}.setValue(key, value);
}

//! The user's defaults, put back when a test ends.
struct DefaultsGuard
{
  QSettings s;
  QVariant editor = s.value(UIPlacementSettings::scriptEditorKey);
  QVariant ui = s.value(UIPlacementSettings::processUIKey);
  QVariant preview = s.value(UIPlacementSettings::scriptEditorPreviewKey);
  ~DefaultsGuard()
  {
    s.setValue(UIPlacementSettings::scriptEditorKey, editor);
    s.setValue(UIPlacementSettings::processUIKey, ui);
    s.setValue(UIPlacementSettings::scriptEditorPreviewKey, preview);
  }
};

//! Where an open editor currently is.
enum class Where
{
  Nowhere,
  Window,
  SidePanel,
  Central,
  CentralOverlay
};

Where whereIs(const score::GUIApplicationContext& ctx, score::Document& doc, QWidget* w)
{
  if(!w)
    return Where::Nowhere;
  if(w->isWindow())
    return Where::Window;
  if(auto v = centralViews(doc).viewContaining(w))
    return v == w ? Where::Central : Where::CentralOverlay;
  if(mainView(ctx).rightTabs->actionFor(w))
    return Where::SidePanel;
  return Where::Nowhere;
}

//! Paints something: enough for the document to report a background.
struct FakeBackground final : score::BackgroundRenderer
{
  bool render(QPainter* painter, const QRectF& rect) override
  {
    painter->fillRect(rect, Qt::magenta);
    return true;
  }
};

Scenario::ScenarioDocumentView& scenarioView(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentView&>(doc.view()->viewDelegate());
}

Scenario::ScenarioDocumentPresenter& scenarioPresenter(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentPresenter&>(
      *doc.presenter()->presenterDelegate());
}

QTabBar& centralTabs(score::Document& doc)
{
  auto bar = centralViews(doc).findChild<QTabBar*>("CentralViewTabs");
  REQUIRE(bar != nullptr);
  return *bar;
}

//! The process's custom UI: a Qt Quick item, so that it can be docked.
constexpr const char* execScript
    = "import Score\nScript { ValueInlet { id: in1 } tick: function(t, s) { } }";
constexpr const char* uiScript
    = "import Score\nScriptUI { implicitWidth: 320; implicitHeight: 200 }";

//! As the edit command does it: the ports the script replaces are only
//! deleted once the presenters have let go of their items.
void setProgram(score::Document&, JS::ProcessModel& js, const char* ui)
{
  const auto res = js.setProgram(JS::QmlSource{execScript, ui});
  REQUIRE(res.valid);
  js.programChanged();
  js.inletsChanged();
  js.outletsChanged();
}

JS::ProcessModel& addJsWithUI(score::Document& doc)
{
  auto& js = static_cast<JS::ProcessModel&>(addJs(doc));
  setProgram(doc, js, uiScript);
  REQUIRE(js.program().ui == uiScript);
  REQUIRE(bool(js.flags() & Process::ProcessFlags::ExternalUIEmbeddable));
  return js;
}

//! The placement menus run their own event loop: whatever popup opens next
//! gets closed right away.
void closeNextPopup()
{
  auto timer = new QTimer;
  timer->setInterval(5);
  QObject::connect(timer, &QTimer::timeout, timer, [timer] {
    if(auto p = QApplication::activePopupWidget())
    {
      p->close();
      timer->deleteLater();
    }
  });
  timer->start();
}

QByteArray saveAsJson(score::Document& doc)
{
  JSONReader w;
  doc.saveAsJson(w);
  return w.toByteArray();
}

score::Document* loadJson(const score::GUIApplicationContext& ctx, QByteArray bytes)
{
  auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
  auto doc = ctx.docManager.loadDocument(
      ctx, QStringLiteral("placement.score"), std::move(bytes), JSONObject::type(),
      *delegates.begin());
  spin(100);
  return doc;
}

Process::ProcessModel* findJs(score::Document& doc)
{
  for(auto& p : baseInterval(doc).processes)
    if(p.concreteKey() == jsKey)
      return &p;
  return nullptr;
}
}

TEST_CASE(
    "A script editor opens where the default says and closes cleanly",
    "[integration][ui][placement][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    setDefault(UIPlacementSettings::scriptEditorPreviewKey, "None");
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& proc = addJs(*doc);
    const int tabsBefore = rightTabCount(ctx);

    const auto check = [&](const char* name, Where expected) {
      setDefault(UIPlacementSettings::scriptEditorKey, name);

      std::vector<bool> visibility;
      QObject::connect(
          &proc, &Process::ProcessModel::scriptUIVisible, &proc,
          [&](bool v) { visibility.push_back(v); });

      Process::setupScriptUI(proc, doc->context(), true);
      spin();
      REQUIRE(proc.scriptUI != nullptr);
      CHECK(whereIs(ctx, *doc, proc.scriptUI) == expected);

      Process::setupScriptUI(proc, doc->context(), false);
      spin();
      CHECK(proc.scriptUI == nullptr);
      CHECK(rightTabCount(ctx) == tabsBefore);
      CHECK(centralViews(*doc).currentView() == centralViews(*doc).mainView());
      CHECK(visibility == std::vector<bool>{true, false});
      QObject::disconnect(
          &proc, &Process::ProcessModel::scriptUIVisible, &proc, nullptr);
    };

    SECTION("side panel")
    {
      check("Side panel", Where::SidePanel);
    }
    SECTION("central view")
    {
      check("Central", Where::Central);
    }
    SECTION("separate window")
    {
      check("Window", Where::Window);
    }
  });
}

TEST_CASE(
    "Opening an editor twice brings the same one back",
    "[integration][ui][placement][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    setDefault(UIPlacementSettings::scriptEditorPreviewKey, "None");
    auto* doc = score::test::new_document(ctx);
    auto& proc = addJs(*doc);

    for(const char* name : {"Side panel", "Central", "Window"})
    {
      setDefault(UIPlacementSettings::scriptEditorKey, name);
      const int tabsBefore = rightTabCount(ctx);

      Process::setupScriptUI(proc, doc->context(), true);
      spin();
      auto first = proc.scriptUI;
      REQUIRE(first != nullptr);
      Process::setupScriptUI(proc, doc->context(), true);
      spin();
      CHECK(proc.scriptUI == first);
      CHECK(
          rightTabCount(ctx)
          == tabsBefore + (QString{name} == QStringLiteral("Side panel") ? 1 : 0));

      Process::setupScriptUI(proc, doc->context(), false);
      spin();
      CHECK(proc.scriptUI == nullptr);
      CHECK(rightTabCount(ctx) == tabsBefore);
    }
  });
}

TEST_CASE(
    "An open editor moves between every placement", "[integration][ui][placement][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    setDefault(UIPlacementSettings::scriptEditorPreviewKey, "None");
    setDefault(UIPlacementSettings::scriptEditorKey, "Side panel");
    auto* doc = score::test::new_document(ctx);
    auto& proc = addJs(*doc);
    const int tabsBefore = rightTabCount(ctx);

    Process::setupScriptUI(proc, doc->context(), true);
    spin();
    auto editor = proc.scriptUI;
    REQUIRE(editor != nullptr);

    const std::pair<const char*, Where> targets[] = {
        {"Central", Where::Central},      {"Window", Where::Window},
        {"Side panel", Where::SidePanel}, {"Central", Where::Central},
        {"Side panel", Where::SidePanel}, {"Window", Where::Window},
        {"Central", Where::Central},
    };
    for(auto [name, where] : targets)
    {
      Process::moveProcessUI(
          proc, doc->context(), Process::ProcessUIKind::ScriptEditor, name);
      spin();
      // Same editor, moved, remembered for the process and as the new default
      CHECK(proc.scriptUI == editor);
      CHECK(whereIs(ctx, *doc, proc.scriptUI) == where);
      CHECK(proc.scriptEditorPlacement() == name);
      CHECK(
          UIPlacementSettings::toString(UIPlacementSettings::scriptEditorPlacement())
          == name);
      CHECK(rightTabCount(ctx) == tabsBefore + (where == Where::SidePanel ? 1 : 0));
    }

    // Back to following the default
    Process::moveProcessUI(
        proc, doc->context(), Process::ProcessUIKind::ScriptEditor, "");
    spin();
    CHECK(proc.scriptEditorPlacement().isEmpty());
    CHECK(whereIs(ctx, *doc, proc.scriptUI) == Where::Central);

    Process::setupScriptUI(proc, doc->context(), false);
    spin();
    CHECK(proc.scriptUI == nullptr);
    CHECK(rightTabCount(ctx) == tabsBefore);
  });
}

namespace
{
//! Regression: hiding then showing again before the previous editor was
//! actually deleted used to corrupt the heap.
void showHideCycles(const score::GUIApplicationContext& ctx, const char* placement)
{
  DefaultsGuard guard;
  auto* doc = score::test::new_document(ctx);
  auto& proc = addJs(*doc);
  const int tabsBefore = rightTabCount(ctx);
  setDefault(UIPlacementSettings::scriptEditorKey, placement);

  for(const char* preview : {"None", "Background"})
  {
    setDefault(UIPlacementSettings::scriptEditorPreviewKey, preview);
    FakeBackground background;
    scenarioView(*doc).addBackgroundRenderer(&background);

    for(int i = 0; i < 4; i++)
    {
      Process::setupScriptUI(proc, doc->context(), true);
      Process::setupScriptUI(proc, doc->context(), false);
    }
    Process::setupScriptUI(proc, doc->context(), true);
    spin(100);
    REQUIRE(proc.scriptUI != nullptr);
    Process::setupScriptUI(proc, doc->context(), false);
    spin(100);
    CHECK(proc.scriptUI == nullptr);
    CHECK(rightTabCount(ctx) == tabsBefore);
    auto cur = centralViews(*doc).currentView();
    INFO(
        "current view: " << (cur ? cur->metaObject()->className() : "null")
                         << " visible " << (cur && cur->isVisible()) << " in stack "
                         << centralViews(*doc).hasView(cur));
    CHECK(cur == centralViews(*doc).mainView());

    scenarioView(*doc).removeBackgroundRenderer(&background);
  }
}
}

TEST_CASE(
    "Show and hide cycles without the event loop in between",
    "[integration][ui][placement][gui]")
{
  SECTION("central view")
  {
    score::test::run_in_gui_app(
        [](const score::GUIApplicationContext& ctx) { showHideCycles(ctx, "Central"); });
  }
  SECTION("side panel")
  {
    score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
      showHideCycles(ctx, "Side panel");
    });
  }
  SECTION("separate window")
  {
    score::test::run_in_gui_app(
        [](const score::GUIApplicationContext& ctx) { showHideCycles(ctx, "Window"); });
  }
}

TEST_CASE(
    "Undoing the creation of a process closes its editor",
    "[integration][ui][placement][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    setDefault(UIPlacementSettings::scriptEditorPreviewKey, "None");
    auto* doc = score::test::new_document(ctx);
    const int tabsBefore = rightTabCount(ctx);

    for(const char* name : {"Side panel", "Central", "Window"})
    {
      setDefault(UIPlacementSettings::scriptEditorKey, name);
      auto& proc = addJs(*doc);
      Process::setupScriptUI(proc, doc->context(), true);
      spin();
      QPointer<QWidget> editor = proc.scriptUI;
      REQUIRE(editor);

      doc->commandStack().undo();
      spin(100);
      CHECK(!editor);
      CHECK(rightTabCount(ctx) == tabsBefore);
      CHECK(centralViews(*doc).currentView() == centralViews(*doc).mainView());
    }
  });
}

TEST_CASE(
    "Closing a document with editors open in every placement",
    "[integration][ui][placement][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    setDefault(UIPlacementSettings::scriptEditorPreviewKey, "None");
    const int tabsBefore = rightTabCount(ctx);

    auto* doc = score::test::new_document(ctx);
    std::vector<QPointer<QWidget>> editors;
    for(const char* name : {"Side panel", "Central", "Window"})
    {
      setDefault(UIPlacementSettings::scriptEditorKey, name);
      auto& proc = addJs(*doc);
      Process::setupScriptUI(proc, doc->context(), true);
      spin();
      REQUIRE(proc.scriptUI != nullptr);
      editors.emplace_back(proc.scriptUI);
    }

    score::test::close_all_documents(ctx);
    spin(100);
    for(auto& e : editors)
      CHECK(!e);
    CHECK(rightTabCount(ctx) == tabsBefore);
  });
}

TEST_CASE(
    "The placement of a process is kept in the document", "[integration][ui][placement]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    auto& proc = addJs(*doc);
    proc.setScriptEditorPlacement("Central");
    proc.setProcessUIPlacement("Window");

    // The binary format
    auto* reloaded = score::test::reload_via_bytes(ctx, *doc);
    REQUIRE(reloaded != nullptr);
    auto copy = findJs(*reloaded);
    REQUIRE(copy != nullptr);
    CHECK(copy->scriptEditorPlacement() == "Central");
    CHECK(copy->processUIPlacement() == "Window");

    // The on-disk format, with and without the keys
    auto* fromJson = loadJson(ctx, saveAsJson(*doc));
    REQUIRE(fromJson != nullptr);
    copy = findJs(*fromJson);
    REQUIRE(copy != nullptr);
    CHECK(copy->scriptEditorPlacement() == "Central");
    CHECK(copy->processUIPlacement() == "Window");

    copy->setScriptEditorPlacement({});
    copy->setProcessUIPlacement({});
    const auto json = saveAsJson(*fromJson);
    CHECK(!json.contains("ScriptEditorPlacement"));
    CHECK(!json.contains("ProcessUIPlacement"));
    auto* plain = loadJson(ctx, json);
    REQUIRE(plain != nullptr);
    copy = findJs(*plain);
    REQUIRE(copy != nullptr);
    CHECK(copy->scriptEditorPlacement().isEmpty());
    CHECK(copy->processUIPlacement().isEmpty());
  });
}

TEST_CASE(
    "The defaults follow the choices made from the menus",
    "[integration][ui][placement]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    auto& settings = ctx.settings<Scenario::Settings::Model>();

    UIPlacementSettings::setScriptEditorPlacement(UIPlacement::Central);
    CHECK(settings.getScriptEditorPlacement() == "Central");
    CHECK(UIPlacementSettings::scriptEditorPlacement() == UIPlacement::Central);

    UIPlacementSettings::setProcessUIPlacement(UIPlacement::Window);
    CHECK(settings.getProcessUIPlacement() == "Window");

    UIPlacementSettings::setScriptEditorPreview(Process::PreviewSource::None);
    CHECK(settings.getScriptEditorPreview() == "None");
    CHECK(UIPlacementSettings::scriptEditorPreview() == Process::PreviewSource::None);

    // Unknown values fall back on the default
    QSettings{}.setValue(UIPlacementSettings::scriptEditorKey, "Somewhere");
    CHECK(
        UIPlacementSettings::scriptEditorPlacement()
        == UIPlacementSettings::defaultScriptEditorPlacement);

    // The settings page goes the other way round
    settings.setScriptEditorPlacement("Window");
    CHECK(UIPlacementSettings::scriptEditorPlacement() == UIPlacement::Window);
    settings.setProcessUIPlacement("Side panel");
    CHECK(UIPlacementSettings::processUIPlacement() == UIPlacement::SidePanel);
    settings.setScriptEditorPreview("Background");
    CHECK(
        UIPlacementSettings::scriptEditorPreview()
        == Process::PreviewSource::Background);
  });
}

TEST_CASE(
    "The placement menu reflects the current choices",
    "[integration][ui][placement][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    auto* doc = score::test::new_document(ctx);
    auto& proc = addJs(*doc);
    proc.setScriptEditorPlacement("Side panel");
    UIPlacementSettings::setScriptEditorPreview(Process::PreviewSource::Background);

    QMenu menu;
    Process::fillPlacementMenu(
        menu, proc, doc->context(), Process::ProcessUIKind::ScriptEditor);

    std::vector<QAction*> checked;
    std::vector<QAction*> checkable;
    for(auto act : menu.actions())
    {
      if(act->isCheckable())
        checkable.push_back(act);
      if(act->isChecked())
        checked.push_back(act);
    }
    // Default + three placements, then two ways to draw the editor
    CHECK(checkable.size() == 4 + 2);
    REQUIRE(checked.size() == 2);
    CHECK(checked[0]->text() == QObject::tr("Side panel"));
    CHECK(checked[1]->text().contains("background"));

    // A UI that cannot be embedded only offers its window: a title and a
    // note, nothing to choose. Not even with a placement asked for.
    proc.setProcessUIPlacement("Central");
    QMenu uiMenu;
    Process::fillPlacementMenu(
        uiMenu, proc, doc->context(), Process::ProcessUIKind::ExternalUI);
    REQUIRE(uiMenu.actions().size() == 2);
    for(auto act : uiMenu.actions())
    {
      CHECK(!act->isCheckable());
      CHECK(!act->isEnabled());
    }

    // Only known names make it into the document
    Process::moveProcessUI(
        proc, doc->context(), Process::ProcessUIKind::ScriptEditor, "Sidepanel");
    CHECK(proc.scriptEditorPlacement() == "Side panel");
  });
}

TEST_CASE(
    "A central editor is chromeless over the document's background only",
    "[integration][ui][placement][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    setDefault(UIPlacementSettings::scriptEditorKey, "Central");
    setDefault(UIPlacementSettings::scriptEditorPreviewKey, "Background");
    auto* doc = score::test::new_document(ctx);
    auto& proc = addJs(*doc);
    auto& views = centralViews(*doc);

    // Nothing behind the document: a plain editor
    Process::setupScriptUI(proc, doc->context(), true);
    spin();
    REQUIRE(proc.scriptUI != nullptr);
    CHECK(whereIs(ctx, *doc, proc.scriptUI) == Where::Central);
    auto bbox = proc.scriptUI->findChild<QDialogButtonBox*>();
    REQUIRE(bbox != nullptr);
    CHECK(!bbox->isHidden());
    Process::setupScriptUI(proc, doc->context(), false);
    spin();

    // A background: the editor sits over it, without its chrome
    FakeBackground background;
    scenarioView(*doc).addBackgroundRenderer(&background);
    REQUIRE(scenarioView(*doc).activeBackgroundRenderer() == &background);

    Process::setupScriptUI(proc, doc->context(), true);
    spin();
    REQUIRE(proc.scriptUI != nullptr);
    CHECK(whereIs(ctx, *doc, proc.scriptUI) == Where::CentralOverlay);
    QPointer<Process::EditorOverlayHost> host
        = qobject_cast<Process::EditorOverlayHost*>(views.viewContaining(proc.scriptUI));
    REQUIRE(host != nullptr);
    CHECK(host->editor() == proc.scriptUI);
    CHECK(qobject_cast<Process::DocumentBackgroundPreview*>(host->preview()) != nullptr);
    bbox = proc.scriptUI->findChild<QDialogButtonBox*>();
    REQUIRE(bbox != nullptr);
    CHECK(bbox->isHidden());
    auto tabs = proc.scriptUI->findChild<QTabWidget*>();
    REQUIRE(tabs != nullptr);
    CHECK(tabs->documentMode());

    // Moving it out of the central view gives the chrome back
    Process::moveProcessUI(
        proc, doc->context(), Process::ProcessUIKind::ScriptEditor, "Side panel");
    spin();
    CHECK(whereIs(ctx, *doc, proc.scriptUI) == Where::SidePanel);
    CHECK(!bbox->isHidden());
    CHECK(!tabs->documentMode());
    CHECK(!host);

    // And back over the background
    Process::moveProcessUI(
        proc, doc->context(), Process::ProcessUIKind::ScriptEditor, "Central");
    spin();
    CHECK(whereIs(ctx, *doc, proc.scriptUI) == Where::CentralOverlay);

    // Choosing the plain editor applies right away
    UIPlacementSettings::setScriptEditorPreview(Process::PreviewSource::None);
    Process::moveProcessUI(
        proc, doc->context(), Process::ProcessUIKind::ScriptEditor, "Central");
    spin();
    CHECK(whereIs(ctx, *doc, proc.scriptUI) == Where::Central);

    Process::setupScriptUI(proc, doc->context(), false);
    spin();
    CHECK(proc.scriptUI == nullptr);
    scenarioView(*doc).removeBackgroundRenderer(&background);
  });
}

TEST_CASE("Ctrl+Return compiles from the editor", "[integration][ui][placement][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    setDefault(UIPlacementSettings::scriptEditorPreviewKey, "None");
    auto* doc = score::test::new_document(ctx);
    auto& proc = addJs(*doc);

    int version = 0;
    const auto compileWith
        = [&](QTextEdit& edit, Qt::Key key, Qt::KeyboardModifiers mods) {
      const int before = doc->commandStack().size();
      edit.setPlainText(QStringLiteral(
                            "import Score\nScript { ValueInlet { id: in1 } "
                            "tick: function(t, s) { } }\n// %1")
                            .arg(++version));
      score::test::keyClick(edit, key, mods);
      spin();
      QString log;
      for(auto pte : proc.scriptUI->findChildren<QPlainTextEdit*>())
        log += pte->toPlainText();
      INFO("editor log: " << log.toStdString());
      CHECK(doc->commandStack().size() == before + 1);
    };

    for(const char* name : {"Window", "Central", "Side panel"})
    {
      setDefault(UIPlacementSettings::scriptEditorKey, name);
      Process::setupScriptUI(proc, doc->context(), true);
      spin();
      REQUIRE(proc.scriptUI != nullptr);
      auto edit = proc.scriptUI->findChild<QTextEdit*>();
      REQUIRE(edit != nullptr);

      // The main Return, the keypad's Enter, and a plain Return which must
      // only insert a line
      compileWith(*edit, Qt::Key_Return, Qt::ControlModifier);
      compileWith(*edit, Qt::Key_Enter, Qt::ControlModifier | Qt::KeypadModifier);
      const int before = doc->commandStack().size();
      score::test::keyClick(*edit, Qt::Key_Return);
      spin();
      CHECK(doc->commandStack().size() == before);

      Process::setupScriptUI(proc, doc->context(), false);
      spin();
    }
  });
}

TEST_CASE(
    "Escape closes a windowed editor and leaves a docked one alone",
    "[integration][ui][placement][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    setDefault(UIPlacementSettings::scriptEditorPreviewKey, "None");
    auto* doc = score::test::new_document(ctx);
    auto& proc = addJs(*doc);

    for(const char* docked : {"Central", "Side panel"})
    {
      setDefault(UIPlacementSettings::scriptEditorKey, docked);
      Process::setupScriptUI(proc, doc->context(), true);
      spin();
      REQUIRE(proc.scriptUI != nullptr);
      score::test::keyClick(*proc.scriptUI, Qt::Key_Escape);
      spin();
      CHECK(proc.scriptUI != nullptr);
      Process::setupScriptUI(proc, doc->context(), false);
      spin();
    }

    setDefault(UIPlacementSettings::scriptEditorKey, "Window");
    Process::setupScriptUI(proc, doc->context(), true);
    spin();
    REQUIRE(proc.scriptUI != nullptr);
    score::test::keyClick(*proc.scriptUI, Qt::Key_Escape);
    spin();
    CHECK(proc.scriptUI == nullptr);
  });
}

TEST_CASE("The navigation bar carries the interval's path", "[integration][ui][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    auto& views = centralViews(*doc);

    auto bar = qobject_cast<Scenario::AddressBarWidget*>(views.navigationWidget());
    REQUIRE(bar != nullptr);
    // One entry: the root interval, named after the document
    CHECK(bar->findChildren<QWidget*>().size() == 1);
    CHECK(views.currentView() == views.mainView());
    CHECK(views.mainView() == doc->view()->viewDelegate().getWidget());

    // Views pushed by plug-ins do not replace the document's own
    setDefault(UIPlacementSettings::scriptEditorKey, "Central");
    auto& proc = addJs(*doc);
    Process::setupScriptUI(proc, doc->context(), true);
    spin();
    CHECK(views.currentView() != views.mainView());
    views.showView(views.mainView());
    CHECK(views.currentView() == views.mainView());
    CHECK(views.hasView(proc.scriptUI));
    Process::setupScriptUI(proc, doc->context(), false);
    spin();
    CHECK(!views.hasView(proc.scriptUI));
  });
}

TEST_CASE(
    "Renaming a process renames its editor's tab", "[integration][ui][placement][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    setDefault(UIPlacementSettings::scriptEditorPreviewKey, "None");
    auto* doc = score::test::new_document(ctx);
    auto& proc = addJs(*doc);

    setDefault(UIPlacementSettings::scriptEditorKey, "Side panel");
    Process::setupScriptUI(proc, doc->context(), true);
    spin();
    auto act = mainView(ctx).rightTabs->actionFor(proc.scriptUI);
    REQUIRE(act != nullptr);
    proc.metadata().setName("Renamed");
    spin();
    CHECK(act->text().startsWith("Renamed"));
    Process::setupScriptUI(proc, doc->context(), false);
    spin();

    setDefault(UIPlacementSettings::scriptEditorKey, "Central");
    Process::setupScriptUI(proc, doc->context(), true);
    spin();
    REQUIRE(centralTabs(*doc).count() == 2);
    proc.metadata().setName("Renamed again");
    spin();
    CHECK(centralTabs(*doc).tabText(1).startsWith("Renamed again"));
    Process::setupScriptUI(proc, doc->context(), false);
    spin();
  });
}

TEST_CASE(
    "A custom UI opens where the default says and closes cleanly",
    "[integration][ui][placement][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    auto* doc = score::test::new_document(ctx);
    auto& proc = addJsWithUI(*doc);
    const int tabsBefore = rightTabCount(ctx);

    const std::pair<const char*, Where> targets[] = {
        {"Side panel", Where::SidePanel},
        {"Central", Where::Central},
        {"Window", Where::Window},
    };
    for(auto [name, where] : targets)
    {
      setDefault(UIPlacementSettings::processUIKey, name);

      std::vector<bool> visibility;
      QObject::connect(
          &proc, &Process::ProcessModel::externalUIVisible, &proc,
          [&](bool v) { visibility.push_back(v); });

      Process::setupExternalUI(proc, doc->context(), true);
      spin();
      REQUIRE(proc.externalUI != nullptr);
      CHECK(whereIs(ctx, *doc, proc.externalUI) == where);
      CHECK(rightTabCount(ctx) == tabsBefore + (where == Where::SidePanel ? 1 : 0));

      // Same UI when asked again
      auto first = proc.externalUI;
      Process::setupExternalUI(proc, doc->context(), true);
      spin();
      CHECK(proc.externalUI == first);

      Process::setupExternalUI(proc, doc->context(), false);
      spin(100);
      CHECK(proc.externalUI == nullptr);
      CHECK(rightTabCount(ctx) == tabsBefore);
      CHECK(centralViews(*doc).currentView() == centralViews(*doc).mainView());
      CHECK(visibility == std::vector<bool>{true, false});
      QObject::disconnect(
          &proc, &Process::ProcessModel::externalUIVisible, &proc, nullptr);
    }
  });
}

TEST_CASE(
    "Reopening a custom UI before the previous one is deleted keeps the new one",
    "[integration][ui][placement][gui]")
{
  // Regression: the deferred deletion of the old window used to take the
  // new UI's item and pointer with it, leaving a tab nothing could close.
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    auto* doc = score::test::new_document(ctx);
    auto& proc = addJsWithUI(*doc);
    const int tabsBefore = rightTabCount(ctx);

    const std::pair<const char*, Where> targets[] = {
        {"Side panel", Where::SidePanel},
        {"Central", Where::Central},
        {"Window", Where::Window},
    };
    for(auto [name, where] : targets)
    {
      setDefault(UIPlacementSettings::processUIKey, name);
      int hidden = 0;
      auto c = QObject::connect(
          &proc, &Process::ProcessModel::externalUIVisible, &proc, [&](bool v) {
        if(!v)
          hidden++;
      });

      for(int i = 0; i < 4; i++)
      {
        Process::setupExternalUI(proc, doc->context(), true);
        Process::setupExternalUI(proc, doc->context(), false);
      }
      Process::setupExternalUI(proc, doc->context(), true);
      QPointer<QWidget> current = proc.externalUI;
      REQUIRE(current);
      spin(100);

      // The last one is still there, in its place, and nothing else is
      CHECK(proc.externalUI == current);
      CHECK(current);
      CHECK(whereIs(ctx, *doc, proc.externalUI) == where);
      CHECK(rightTabCount(ctx) == tabsBefore + (where == Where::SidePanel ? 1 : 0));
      CHECK(centralTabs(*doc).count() == (where == Where::Central ? 2 : 1));
      CHECK(hidden == 4);

      // A new compile goes to the UI that is shown
      setProgram(
          *doc, proc,
          "import Score\nScriptUI { implicitWidth: 300; implicitHeight: 300 }");
      spin(100);
      CHECK(proc.externalUI == current);
      CHECK(whereIs(ctx, *doc, proc.externalUI) == where);

      Process::setupExternalUI(proc, doc->context(), false);
      spin(100);
      CHECK(proc.externalUI == nullptr);
      CHECK(!current);
      CHECK(rightTabCount(ctx) == tabsBefore);
      CHECK(centralTabs(*doc).count() == 1);
      CHECK(hidden == 5);
      QObject::disconnect(c);
    }
  });
}

TEST_CASE(
    "Moving an editor while a previous one is still to be deleted",
    "[integration][ui][placement][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    setDefault(UIPlacementSettings::scriptEditorPreviewKey, "None");
    setDefault(UIPlacementSettings::scriptEditorKey, "Side panel");
    auto* doc = score::test::new_document(ctx);
    auto& proc = addJs(*doc);
    auto& rightTabs = *mainView(ctx).rightTabs;
    const int tabsBefore = rightTabCount(ctx);

    for(const char* target : {"Side panel", "Central", "Side panel"})
    {
      Process::setupScriptUI(proc, doc->context(), true);
      Process::setupScriptUI(proc, doc->context(), false);
      Process::setupScriptUI(proc, doc->context(), true);
      Process::moveProcessUI(
          proc, doc->context(), Process::ProcessUIKind::ScriptEditor, target);
      spin(100);

      REQUIRE(proc.scriptUI != nullptr);
      const bool side = QString{target} == QStringLiteral("Side panel");
      CHECK(
          whereIs(ctx, *doc, proc.scriptUI)
          == (side ? Where::SidePanel : Where::Central));
      CHECK(rightTabCount(ctx) == tabsBefore + (side ? 1 : 0));
      // What was just shown stays shown: the deferred fallback of the
      // removed tab must not put the inspector back in front
      if(side)
        CHECK(rightTabs.currentWidget() == proc.scriptUI);
      else
        CHECK(centralViews(*doc).currentView() == proc.scriptUI);

      Process::setupScriptUI(proc, doc->context(), false);
      spin(100);
      CHECK(proc.scriptUI == nullptr);
      CHECK(rightTabCount(ctx) == tabsBefore);
    }
  });
}

TEST_CASE(
    "Closing the central tab closes the editor", "[integration][ui][placement][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    setDefault(UIPlacementSettings::scriptEditorKey, "Central");
    auto* doc = score::test::new_document(ctx);
    auto& proc = addJs(*doc);
    auto& views = centralViews(*doc);
    FakeBackground background;
    scenarioView(*doc).addBackgroundRenderer(&background);

    for(const char* preview : {"None", "Background"})
    {
      setDefault(UIPlacementSettings::scriptEditorPreviewKey, preview);
      Process::setupScriptUI(proc, doc->context(), true);
      spin();
      REQUIRE(proc.scriptUI != nullptr);
      QPointer<QWidget> editor = proc.scriptUI;
      QPointer<QWidget> view = views.viewContaining(editor);
      REQUIRE(view);
      REQUIRE(centralTabs(*doc).count() == 2);

      centralTabs(*doc).tabCloseRequested(1);
      spin(100);
      CHECK(proc.scriptUI == nullptr);
      CHECK(!editor);
      CHECK(!view);
      CHECK(centralTabs(*doc).count() == 1);
      CHECK(views.currentView() == views.mainView());
    }
    scenarioView(*doc).removeBackgroundRenderer(&background);
  });
}

TEST_CASE(
    "Moving the custom UI leaves an overlaid editor in place",
    "[integration][ui][placement][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    setDefault(UIPlacementSettings::scriptEditorKey, "Central");
    setDefault(UIPlacementSettings::scriptEditorPreviewKey, "Background");
    setDefault(UIPlacementSettings::processUIKey, "Central");
    auto* doc = score::test::new_document(ctx);
    auto& proc = addJsWithUI(*doc);
    auto& views = centralViews(*doc);
    FakeBackground background;
    scenarioView(*doc).addBackgroundRenderer(&background);

    Process::setupScriptUI(proc, doc->context(), true);
    Process::setupExternalUI(proc, doc->context(), true);
    spin();
    REQUIRE(proc.scriptUI != nullptr);
    REQUIRE(proc.externalUI != nullptr);
    CHECK(whereIs(ctx, *doc, proc.scriptUI) == Where::CentralOverlay);
    CHECK(whereIs(ctx, *doc, proc.externalUI) == Where::Central);
    QPointer<QWidget> host = views.viewContaining(proc.scriptUI);
    REQUIRE(host);
    CHECK(centralTabs(*doc).count() == 3);

    Process::moveProcessUI(
        proc, doc->context(), Process::ProcessUIKind::ExternalUI, "Side panel");
    spin();
    CHECK(whereIs(ctx, *doc, proc.externalUI) == Where::SidePanel);
    CHECK(whereIs(ctx, *doc, proc.scriptUI) == Where::CentralOverlay);
    CHECK(host);
    CHECK(views.viewContaining(proc.scriptUI) == host);
    CHECK(centralTabs(*doc).count() == 2);

    Process::setupExternalUI(proc, doc->context(), false);
    Process::setupScriptUI(proc, doc->context(), false);
    spin(100);
    CHECK(proc.scriptUI == nullptr);
    CHECK(proc.externalUI == nullptr);
    CHECK(!host);
    scenarioView(*doc).removeBackgroundRenderer(&background);
  });
}

TEST_CASE(
    "Closing a document with an editor over its background",
    "[integration][ui][placement][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    setDefault(UIPlacementSettings::scriptEditorKey, "Central");
    setDefault(UIPlacementSettings::scriptEditorPreviewKey, "Background");
    setDefault(UIPlacementSettings::processUIKey, "Central");
    const int tabsBefore = rightTabCount(ctx);

    auto* doc = score::test::new_document(ctx);
    auto& proc = addJsWithUI(*doc);
    FakeBackground background;
    scenarioView(*doc).addBackgroundRenderer(&background);

    Process::setupScriptUI(proc, doc->context(), true);
    Process::setupExternalUI(proc, doc->context(), true);
    spin();
    REQUIRE(whereIs(ctx, *doc, proc.scriptUI) == Where::CentralOverlay);
    QPointer<QWidget> editor = proc.scriptUI;
    QPointer<QWidget> ui = proc.externalUI;
    QPointer<QWidget> host = centralViews(*doc).viewContaining(editor);
    REQUIRE(host);

    // The processes go before the view: the editor, closed along with the
    // view, must not report to a process that is gone
    score::test::close_all_documents(ctx);
    spin(100);
    CHECK(!editor);
    CHECK(!ui);
    CHECK(!host);
    CHECK(rightTabCount(ctx) == tabsBefore);
  });
}

TEST_CASE(
    "Right-clicking a tab asks for the placement menu",
    "[integration][ui][placement][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    setDefault(UIPlacementSettings::scriptEditorPreviewKey, "None");
    auto* doc = score::test::new_document(ctx);
    auto& proc = addJs(*doc);
    auto& views = centralViews(*doc);
    auto& rightTabs = *mainView(ctx).rightTabs;

    // Central: the tab bar's own context menu event
    setDefault(UIPlacementSettings::scriptEditorKey, "Central");
    Process::setupScriptUI(proc, doc->context(), true);
    spin();
    REQUIRE(proc.scriptUI != nullptr);
    {
      QWidget* asked{};
      auto c = QObject::connect(
          &views, &score::CentralViewStack::viewContextMenuRequested, &views,
          [&](QWidget* v, QPoint) { asked = v; });
      auto& bar = centralTabs(*doc);
      const QPoint pos = bar.tabRect(1).center();
      closeNextPopup();
      QContextMenuEvent ev{QContextMenuEvent::Mouse, pos, bar.mapToGlobal(pos)};
      QCoreApplication::sendEvent(&bar, &ev);
      spin();
      CHECK(asked == proc.scriptUI);

      // The document's own tab has no menu
      asked = nullptr;
      const QPoint mainPos = bar.tabRect(0).center();
      QContextMenuEvent mainEv{
          QContextMenuEvent::Mouse, mainPos, bar.mapToGlobal(mainPos)};
      QCoreApplication::sendEvent(&bar, &mainEv);
      spin();
      CHECK(asked == nullptr);
      QObject::disconnect(c);
    }
    Process::setupScriptUI(proc, doc->context(), false);
    spin();

    // Side panel: the tab's button
    setDefault(UIPlacementSettings::scriptEditorKey, "Side panel");
    Process::setupScriptUI(proc, doc->context(), true);
    spin();
    REQUIRE(proc.scriptUI != nullptr);
    {
      QWidget* asked{};
      auto c = QObject::connect(
          &rightTabs, &score::FixedTabWidget::tabContextMenuRequested, &rightTabs,
          [&](QWidget* w, QPoint) { asked = w; });
      auto act = rightTabs.actionFor(proc.scriptUI);
      REQUIRE(act != nullptr);
      auto button = rightTabs.toolbar()->widgetForAction(act);
      REQUIRE(button != nullptr);
      const QPoint pos = button->rect().center();
      closeNextPopup();
      QContextMenuEvent ev{QContextMenuEvent::Mouse, pos, button->mapToGlobal(pos)};
      QCoreApplication::sendEvent(button, &ev);
      spin();
      CHECK(asked == proc.scriptUI);
      QObject::disconnect(c);
    }
    Process::setupScriptUI(proc, doc->context(), false);
    spin();
  });
}

TEST_CASE(
    "Moving a tab out from its own context menu", "[integration][ui][placement][gui]")
{
  // The menu runs from the tab button's event handler; the move deletes
  // that button, which must wait until the handler is done with it.
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    setDefault(UIPlacementSettings::scriptEditorPreviewKey, "None");
    setDefault(UIPlacementSettings::scriptEditorKey, "Side panel");
    auto* doc = score::test::new_document(ctx);
    auto& proc = addJs(*doc);
    auto& rightTabs = *mainView(ctx).rightTabs;
    const int tabsBefore = rightTabCount(ctx);

    Process::setupScriptUI(proc, doc->context(), true);
    spin();
    REQUIRE(proc.scriptUI != nullptr);
    auto act = rightTabs.actionFor(proc.scriptUI);
    REQUIRE(act != nullptr);
    auto button = rightTabs.toolbar()->widgetForAction(act);
    REQUIRE(button != nullptr);

    // Pick "Central view" from the popup once it shows
    auto timer = new QTimer;
    timer->setInterval(5);
    QObject::connect(timer, &QTimer::timeout, timer, [timer] {
      auto menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      if(!menu)
        return;
      for(auto a : menu->actions())
      {
        if(a->text() == QObject::tr("Central view"))
        {
          a->trigger();
          break;
        }
      }
      menu->close();
      timer->deleteLater();
    });
    timer->start();

    const QPoint pos = button->rect().center();
    QContextMenuEvent ev{QContextMenuEvent::Mouse, pos, button->mapToGlobal(pos)};
    QCoreApplication::sendEvent(button, &ev);
    spin(100);

    CHECK(whereIs(ctx, *doc, proc.scriptUI) == Where::Central);
    CHECK(proc.scriptEditorPlacement() == "Central");
    CHECK(rightTabCount(ctx) == tabsBefore);
    Process::setupScriptUI(proc, doc->context(), false);
    spin();
  });
}

TEST_CASE("The address bar brings the score back", "[integration][ui][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    DefaultsGuard guard;
    setDefault(UIPlacementSettings::scriptEditorPreviewKey, "None");
    setDefault(UIPlacementSettings::scriptEditorKey, "Central");
    auto* doc = score::test::new_document(ctx);
    auto& views = centralViews(*doc);
    auto& presenter = scenarioPresenter(*doc);
    auto& base = baseInterval(*doc);
    auto bar = qobject_cast<Scenario::AddressBarWidget*>(views.navigationWidget());
    REQUIRE(bar != nullptr);

    // An interval inside the root scenario
    auto scenario = qobject_cast<Scenario::ProcessModel*>(&*base.processes.begin());
    REQUIRE(scenario != nullptr);
    auto& startState = scenario->states.at(scenario->startEvent().states().front());
    CommandDispatcher<> disp{doc->context().commandStack};
    auto cmd = new Scenario::Command::CreateInterval_State_Event_TimeSync{
        *scenario, startState.id(), TimeVal::fromMsecs(2000), 0.5, false};
    disp.submit(cmd);
    auto& child = scenario->intervals.at(cmd->createdInterval());

    presenter.setDisplayedInterval(&child);
    spin();
    CHECK(&presenter.displayedInterval() == &child);
    // Root, separator, child
    REQUIRE(bar->findChildren<QWidget*>().size() == 3);

    // An editor in front, then a trip up the path: the score comes back
    auto& proc = addJs(*doc);
    Process::setupScriptUI(proc, doc->context(), true);
    spin();
    REQUIRE(proc.scriptUI != nullptr);
    CHECK(views.currentView() != views.mainView());

    auto root = bar->findChildren<QWidget*>().front();
    score::test::mouseClick(*root, root->rect().center());
    spin();
    CHECK(views.currentView() == views.mainView());
    CHECK(&presenter.displayedInterval() == &base);
    CHECK(bar->findChildren<QWidget*>().size() == 1);
    // The editor is still open behind
    CHECK(proc.scriptUI != nullptr);
    CHECK(views.hasView(proc.scriptUI));

    Process::setupScriptUI(proc, doc->context(), false);
    spin();
  });
}

TEST_CASE(
    "A document created before the window has a size still fits its interval",
    "[integration][ui][gui]")
{
  // The document widget is laid out after the presenter's deferred setup
  // computes the zoom: the first resize must compute it for real.
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto& window = mainView(ctx);
    auto* doc = score::test::new_document(ctx);
    auto& presenter = scenarioPresenter(*doc);
    spin();

    window.resize(1200, 750);
    window.show();
    spin(200);

    const auto zoom = presenter.zoomRatio();
    INFO("zoom " << zoom);
    REQUIRE(std::isfinite(zoom));
    REQUIRE(zoom > 0);
    const auto viewport = scenarioView(*doc).viewportRect().width();
    REQUIRE(viewport > 0);
    // The interval is at least as wide as what is shown
    const auto width
        = presenter.displayedInterval().duration.guiDuration().toPixels(zoom);
    CHECK(width >= viewport);
    // What is stored in the document is sound too
    CHECK(std::isfinite(presenter.displayedInterval().zoom()));

    window.hide();
    spin();
  });
}
