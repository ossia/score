// References to published objects through the widgets, the broken-references
// panel, the selection and a running Javascript process.

#include <State/Address.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/State/MessageNode.hpp>

#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Inspector/InspectorWidgetList.hpp>
#include <JS/Executor/CPUNode.hpp>
#include <LocalTree/LocalTreeDocumentPlugin.hpp>
#include <LocalTree/ReferencesDialog.hpp>
#include <LocalTree/ScriptableProcessComponent.hpp>
#include <LocalTree/ScriptableReference.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/selection/SelectionDispatcher.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/token_request.hpp>

#include <QCheckBox>
#include <QElapsedTimer>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QLineEdit>
#include <QTreeWidget>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

namespace
{
const QString smooth_uuid = QStringLiteral("bf603921-5a48-4aa5-9bc1-48a762be6467");

void spin(int ms = 0)
{
  QApplication::processEvents();
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  QApplication::processEvents();
}

Process::ControlInlet& control(Process::ProcessModel& p, const QString& name)
{
  for(auto inlet : p.inlets())
    if(auto ctl = qobject_cast<Process::ControlInlet*>(inlet);
       ctl && ctl->name() == name)
      return *ctl;
  FAIL("no control named " << name.toStdString());
  throw;
}

Scenario::StateModel& some_state(score::Document& doc)
{
  auto& itv = score::test::base_interval(doc);
  for(auto& proc : itv.processes)
    if(auto* s = qobject_cast<Scenario::ProcessModel*>(&proc))
      return *s->states.begin();
  FAIL("no scenario in the base interval");
  throw;
}

State::MessageList messages(Scenario::StateModel& state)
{
  return Process::flatten(state.messages().rootNode());
}

QCheckBox* scriptableBox(QWidget& widget, const QString& nameShown)
{
  for(auto edit : widget.findChildren<QLineEdit*>())
    if(edit->text() == nameShown)
      if(auto box = edit->parentWidget()->findChild<QCheckBox*>())
        return box;
  return nullptr;
}

QLineEdit* scriptingNameEdit(QWidget& widget, const QString& nameShown)
{
  for(auto edit : widget.findChildren<QLineEdit*>())
    if(edit->text() == nameShown && edit->placeholderText() == "Scripting name")
      return edit;
  return nullptr;
}

// Other items for the same port than the registered one are inert
Dataflow::PortItem* portItem(const Process::Context& ctx, const Process::Port& p)
{
  auto& map = ctx.dataflow.ports();
  auto it = map.find(&p);
  return it != map.end() ? it->second : nullptr;
}
}

TEST_CASE(
    "the port list publishes a control and names it; the process inspector renames "
    "the process and the state message follows",
    "[integration][scriptable][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    const auto& dctx = doc->context();

    Process::PortListWidget list{*proc, dctx, nullptr};
    auto box = scriptableBox(list, QStringLiteral("amount"));
    REQUIRE(box);
    REQUIRE(!box->isChecked());
    box->click();
    spin();
    REQUIRE(ctl.scriptable());
    REQUIRE(LocalTree::scriptableAddress(ctl).isSet());

    auto name = scriptingNameEdit(list, QStringLiteral("amount"));
    REQUIRE(name);
    name->setText(QStringLiteral("wet"));
    name->editingFinished();
    spin();
    REQUIRE(ctl.exposed() == "wet");
    REQUIRE(LocalTree::scriptableAddress(ctl).path.last() == "wet");

    auto& state = some_state(*doc);
    CommandDispatcher<>{dctx.commandStack}.submit<Scenario::Command::AddMessagesToState>(
        state, State::MessageList{State::Message{
                   State::AddressAccessor{LocalTree::scriptableAddress(ctl)}, 0.5f}});
    REQUIRE(messages(state)[0].address.address.anchored());

    auto& inspectors = ctx.interfaces<Inspector::InspectorWidgetList>();
    auto widgets = inspectors.make(dctx, {proc}, nullptr);
    REQUIRE(!widgets.empty());
    QLineEdit* procName{};
    for(auto w : widgets)
      if(auto e = scriptingNameEdit(*w, proc->metadata().getName()))
        procName = e;
    REQUIRE(procName);
    procName->setText(QStringLiteral("reverb"));
    procName->editingFinished();
    spin();
    REQUIRE(proc->metadata().getName() == "reverb");
    REQUIRE(
        messages(state)[0].address.address.path
        == QStringList{"controls", "reverb", "wet"});

    doc->commandStack().undo();
    spin();
    REQUIRE(messages(state)[0].address.address.path.at(1) != "reverb");
    for(auto w : widgets)
      delete w;
  });
}

TEST_CASE(
    "the broken references panel lists what points at nothing, and empties when it "
    "is published again",
    "[integration][scriptable][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    const auto& dctx = doc->context();
    CommandDispatcher<> disp{dctx.commandStack};

    auto& state = some_state(*doc);
    disp.submit<Scenario::Command::AddMessagesToState>(
        state, State::MessageList{State::Message{
                   *State::parseAddressAccessor("score:/controls/fx/amount"), 0.5f}});

    LocalTree::ReferencesDialog panel{ctx, nullptr};
    panel.setDocument(&dctx);
    auto tree = panel.findChild<QTreeWidget*>();
    REQUIRE(tree);
    spin();
    REQUIRE(tree->topLevelItemCount() == 1);
    REQUIRE(tree->topLevelItem(0)->text(0) == "score:/controls/fx/amount");

    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
    disp.submit<Process::SetPortScriptable>(
        control(*proc, QStringLiteral("Amount")), true);
    spin();
    REQUIRE(tree->topLevelItemCount() == 0);
    REQUIRE(messages(state)[0].address.address.anchored());

    // Undoing back past the process creation lists the reference as broken again
    for(int i = 0; i < 3; i++)
      doc->commandStack().undo();
    spin();
    REQUIRE(tree->topLevelItemCount() == 1);
    panel.setDocument(nullptr);
  });
}

TEST_CASE(
    "selecting a state lights the port items it drives",
    "[integration][scriptable][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    const auto& dctx = doc->context();
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    CommandDispatcher<> disp{dctx.commandStack};
    disp.submit<Process::SetPortScriptable>(ctl, true);
    auto& state = some_state(*doc);
    disp.submit<Scenario::Command::AddMessagesToState>(
        state, State::MessageList{State::Message{
                   State::AddressAccessor{LocalTree::scriptableAddress(ctl)}, 0.5f}});

    auto presenter
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
            *doc);
    REQUIRE(presenter);
    QGraphicsScene scene;
    auto root = new QGraphicsRectItem;
    scene.addItem(root);
    auto& pctx = presenter->context();
    if(!portItem(pctx, ctl))
      new Process::DefaultEffectItem{false, *proc, pctx, root};
    spin(50);

    auto amount = portItem(pctx, ctl);
    REQUIRE(amount);
    REQUIRE(!amount->highlighted());
    std::vector<Dataflow::PortItem*> others;
    for(auto& inl : proc->inlets())
      if(inl != &ctl)
        if(auto item = portItem(pctx, *inl))
          others.push_back(item);
    REQUIRE(!others.empty());

    auto& tree = dctx.plugin<LocalTree::DocumentPlugin>();
    REQUIRE(tree.targets(state) == std::vector<QObject*>{&ctl});
    score::SelectionDispatcher{dctx.selectionStack}.select(state);
    spin();
    REQUIRE(tree.emphasized(ctl));
    REQUIRE(amount->highlighted());
    for(auto p : others)
      REQUIRE(!p->highlighted());

    score::SelectionDispatcher{dctx.selectionStack}.deselect();
    spin();
    REQUIRE(!amount->highlighted());
  });
}

TEST_CASE(
    "a running Javascript process reads a published control through Controls",
    "[integration][scriptable][js]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    const auto& dctx = doc->context();
    auto proc = score::test::add_process(*doc, smooth_uuid, {});
    REQUIRE(proc);
    auto& ctl = control(*proc, QStringLiteral("Amount"));
    CommandDispatcher<> disp{dctx.commandStack};
    disp.submit<Process::RenameProcess>(*proc, QStringLiteral("fx"));
    disp.submit<Process::SetPortScriptable>(ctl, true);
    disp.submit<Process::SetPortScriptingName>(ctl, QStringLiteral("wet"));
    disp.submit<Process::SetValue>(ctl, ossia::value{0.25f});
    spin();

    auto& tree = dctx.plugin<LocalTree::DocumentPlugin>();
    REQUIRE(tree.snapshot()->entries.count("controls/fx/wet") == 1);

    // Both import forms: the qualified one reaches the Score module's singletons
    struct Case
    {
      const char* script;
      bool outlet;
    };
    for(auto [script, outlet] : {Case{R"_(import Score
Script {
  property real seen: -1
  ValueOutlet { id: out; objectName: "out" }
  tick: function(token, state) {
    seen = Score.Controls.fx.wet;
    out.value = Score.Controls.fx.wet;
  }
})_", true}, Case{R"_(import Score as Score
Score.Script {
  property real seen: -1
  tick: function(token, state) {
    seen = Score.Controls.fx.wet;
  }
})_", false}})
    {
      INFO(script);
      ossia::execution_state st;
      st.sampleRate = 48000;
      st.bufferSize = 64;
      st.register_device(&tree.device());
      st.apply_device_changes();
      auto node = std::make_shared<JS::js_node>(st, tree.snapshot());
      struct Clear
      {
        JS::js_node& node;
        ~Clear() { node.clear(); }
      } clear{*node};
      node->root_outputs().push_back(new ossia::value_outlet);
      node->setScript({}, QString::fromUtf8(script));

      ossia::token_request tk;
      tk.prev_date = ossia::time_value{0};
      tk.date = ossia::time_value{705600000 / 48000 * 64};
      tk.parent_duration = ossia::time_value{705600000};
      tk.speed = 1.;
      tk.tempo = 120.;
      tk.signature = ossia::time_signature{4, 4};
      tk.start_sample = 0;
      tk.length_sample = 64;
      node->run(tk, ossia::exec_state_facade{&st});

      REQUIRE(node->m_object);
      REQUIRE(node->m_object->property("seen").toFloat() == Catch::Approx(0.25f));
      if(!outlet)
        continue;
      auto& out = *node->root_outputs()[0]->target<ossia::value_port>();
      REQUIRE(!out.get_data().empty());
      REQUIRE(ossia::convert<float>(out.get_data().back().value) == Catch::Approx(0.25f));
    }
  });
}
