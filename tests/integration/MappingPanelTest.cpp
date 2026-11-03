// The Connections panel lists the cables of a document and the ports bound to
// addresses, follows every edit, selects what is clicked and removes through
// undoable commands.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Keyboard.hpp>
#include <score_test/Mouse.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/Commands/Remove.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Dataflow/Commands/EditConnection.hpp>
#include <Dataflow/MappingPanel/ConnectionsPanel.hpp>
#include <Dataflow/MappingPanel/ConnectionsWidget.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#include <QComboBox>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QTreeView>

#include <catch2/catch_test_macros.hpp>

namespace
{
using Dataflow::ConnectionRow;
using Dataflow::ConnectionsWidget;

// Media::Merger: one audio outlet, eight audio inlets
const UuidKey<Process::ProcessModel> mergerKey
    = UuidKey<Process::ProcessModel>::fromString(
        QStringLiteral("fa02fc21-80ab-41e7-a3c9-5d2fa34ecda7"));

Process::ProcessModel& addMerger(score::Document& doc, Scenario::IntervalModel& itv)
{
  Scenario::Command::Macro m{
      new Scenario::Command::DropProcessInIntervalMacro, doc.context()};
  auto* proc = m.createProcessInNewSlot(itv, mergerKey, {});
  m.commit();
  REQUIRE(proc != nullptr);
  return *proc;
}

void spin()
{
  for(int i = 0; i < 5; i++)
    QApplication::processEvents();
}

int count(const ConnectionsWidget& w, ConnectionRow::Kind k)
{
  return (int)std::count_if(w.rows().begin(), w.rows().end(), [k](auto& r) {
    return r.kind == k;
  });
}

int rowOfCable(const ConnectionsWidget& w, const Process::Cable& c)
{
  for(int i = 0; i < std::ssize(w.rows()); i++)
    if(w.rows()[i].cable == &c)
      return i;
  return -1;
}

int rowOfPort(const ConnectionsWidget& w, const Process::Port& p)
{
  for(int i = 0; i < std::ssize(w.rows()); i++)
    if(w.rows()[i].kind == ConnectionRow::Address && w.rows()[i].sourcePort == &p)
      return i;
  return -1;
}

//! Leaf rows the view shows, groups excluded
int visibleLeaves(const ConnectionsWidget& w, const QModelIndex& parent = {})
{
  auto m = w.view().model();
  int n = 0;
  for(int i = 0; i < m->rowCount(parent); i++)
  {
    auto idx = m->index(i, 0, parent);
    if(w.rowAt(idx) >= 0)
      n++;
    n += visibleLeaves(w, idx);
  }
  return n;
}

void clickRow(ConnectionsWidget& w, int row)
{
  const auto idx = w.viewIndexOf(row);
  REQUIRE(idx.isValid());
  w.view().scrollTo(idx);
  const auto rect = w.view().visualRect(idx.siblingAtColumn(ConnectionsWidget::FromColumn));
  REQUIRE(rect.isValid());
  score::test::mouseClick(*w.view().viewport(), rect.center());
  spin();
}
}

TEST_CASE("Connections panel: listing and live updates", "[integration][gui][dataflow]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& dm
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate());
    auto& itv = dm.baseInterval();
    auto& stack = doc->commandStack();

    auto& a = addMerger(*doc, itv);
    auto& b = addMerger(*doc, itv);
    a.metadata().setName("Source merger");
    b.metadata().setName("Sink merger");
    auto& out = *a.outlets().front();
    auto& in = *b.inlets()[2];


    ConnectionsWidget w;
    w.resize(900, 400);
    w.setDocument(&doc->context());
    w.show();
    spin();

    const int baseCables = count(w, ConnectionRow::Cable);
    const int baseAddresses = count(w, ConnectionRow::Address);
    CHECK(baseCables == int(dm.cables.size()));

    CommandDispatcher<> disp{doc->context().commandStack};
    const auto cid = getStrongId(dm.cables);
    disp.submit<Dataflow::CreateCable>(
        dm, cid, Process::CableType::ImmediateGlutton, out, in);
    auto& cable = dm.cables.at(cid);

    // Coalesced: one rebuild on the next event loop iteration
    CHECK(w.refreshPending());
    spin();
    CHECK(!w.refreshPending());
    REQUIRE(count(w, ConnectionRow::Cable) == baseCables + 1);

    // The panel is registered and follows the current document
    auto panel = ctx.findPanel<Dataflow::ConnectionsPanel>();
    REQUIRE(panel != nullptr);
    panel->connections().refresh();
    CHECK(panel->connections().rows().size() == w.rows().size());
    {
      const int row = rowOfCable(w, cable);
      REQUIRE(row >= 0);
      const auto& r = w.rows()[row];
      CHECK(r.from.contains("Source merger"));
      CHECK(r.to.contains("Sink merger"));
      CHECK(r.to.endsWith(in.visualName().isEmpty() ? QString("Inlet 3") : in.visualName() + " 3"));
      CHECK(r.sourceProcess == &a);
      CHECK(r.sinkProcess == &b);
      CHECK(r.portType == Process::PortType::Audio);
      CHECK(r.mode == Dataflow::cableTypeName(int(Process::CableType::ImmediateGlutton)));
      CHECK(w.viewIndexOf(row).isValid());
    }

    // Changing the cable mode through its command shows up
    disp.submit<Dataflow::UpdateCable>(cable, Process::CableType::DelayedStrict);
    spin();
    CHECK(
        w.rows()[rowOfCable(w, cable)].mode
        == Dataflow::cableTypeName(int(Process::CableType::DelayedStrict)));

    // Binding a port to an address that no device has
    auto& bound = *b.inlets()[5];
    disp.submit<Process::ChangePortAddress>(
        bound, State::AddressAccessor{State::Address{"nodevice", {"foo", "bar"}}});
    spin();
    REQUIRE(count(w, ConnectionRow::Address) == baseAddresses + 1);
    {
      const auto& r = w.rows()[rowOfPort(w, bound)];
      CHECK(r.addressIsSource);
      CHECK(r.from == "nodevice:/foo/bar");
      CHECK(r.to.contains("Sink merger"));
      CHECK(r.device == "nodevice");
      CHECK(r.status == ConnectionRow::MissingDevice);
    }

    // A pattern, a node the device lacks, and an address that resolves
    disp.submit<Process::ChangePortAddress>(
        bound, State::AddressAccessor{State::Address{"nodevice", {"foo", "*"}}});
    spin();
    CHECK(w.rows()[rowOfPort(w, bound)].status == ConnectionRow::Pattern);

    auto& devices = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto local = devices.list().localDevice();
    REQUIRE(local != nullptr);
    disp.submit<Process::ChangePortAddress>(
        bound, State::AddressAccessor{State::Address{local->name(), {"not", "here"}}});
    spin();
    CHECK(w.rows()[rowOfPort(w, bound)].status == ConnectionRow::MissingNode);
    {
      disp.submit<Process::ChangePortAddress>(
          bound, State::AddressAccessor{State::Address{local->name(), {}}});
      spin();
      CHECK(w.rows()[rowOfPort(w, bound)].status == ConnectionRow::Resolved);
    }

    // Renaming a process shows up without a command
    a.metadata().setName("Renamed merger");
    spin();
    CHECK(w.rows()[rowOfCable(w, cable)].from.contains("Renamed merger"));

    // Search and kind filters
    w.searchField().setText("Renamed merger");
    spin();
    CHECK(visibleLeaves(w) == 1);
    w.searchField().clear();
    w.kindBox().setCurrentIndex(ConnectionsWidget::AddressesOnly);
    spin();
    CHECK(visibleLeaves(w) == count(w, ConnectionRow::Address));
    w.kindBox().setCurrentIndex(ConnectionsWidget::AllKinds);
    w.groupingBox().setCurrentIndex(ConnectionsWidget::ByKind);
    spin();
    CHECK(visibleLeaves(w) == std::ssize(w.rows()));
    CHECK(w.view().model()->rowCount() <= 2);
    w.groupingBox().setCurrentIndex(ConnectionsWidget::NoGrouping);
    spin();

    // Removing the cable elsewhere; undo brings it back
    disp.submit<Dataflow::RemoveCable>(dm, cable);
    spin();
    CHECK(count(w, ConnectionRow::Cable) == baseCables);
    stack.undo();
    spin();
    CHECK(count(w, ConnectionRow::Cable) == baseCables + 1);

    // Undoing the bindings unlists them
    stack.undo(); // resolved
    stack.undo(); // missing node
    stack.undo(); // pattern
    stack.undo(); // missing device
    spin();
    CHECK(count(w, ConnectionRow::Address) == baseAddresses);
    CHECK(rowOfPort(w, bound) == -1);

    // Removing a process takes its cables away
    disp.submit<Scenario::Command::RemoveProcessFromInterval>(itv, b.id());
    spin();
    CHECK(count(w, ConnectionRow::Cable) == baseCables);
    stack.undo();
    spin();
    CHECK(count(w, ConnectionRow::Cable) == baseCables + 1);

    // Another document, then none
    score::Document* other = score::test::new_document(ctx);
    REQUIRE(other != nullptr);
    w.setDocument(&other->context());
    spin();
    CHECK(count(w, ConnectionRow::Cable) == 0);
    w.setDocument(nullptr);
    spin();
    CHECK(w.rows().empty());
  });
}

TEST_CASE(
    "Connections panel: select on click, delete and undo",
    "[integration][gui][dataflow][undo]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& dm
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate());
    auto& itv = dm.baseInterval();
    auto& stack = doc->commandStack();
    auto& sel = doc->context().selectionStack;

    auto& a = addMerger(*doc, itv);
    auto& b = addMerger(*doc, itv);
    CommandDispatcher<> disp{doc->context().commandStack};
    const auto cid = getStrongId(dm.cables);
    disp.submit<Dataflow::CreateCable>(
        dm, cid, Process::CableType::ImmediateGlutton, *a.outlets().front(),
        *b.inlets().front());
    auto& cable = dm.cables.at(cid);
    auto& bound = *b.inlets()[1];
    disp.submit<Process::ChangePortAddress>(
        bound, State::AddressAccessor{State::Address{"nodevice", {"x"}}});

    ConnectionsWidget w;
    w.resize(900, 400);
    w.setDocument(&doc->context());
    score::test::showAndActivate(w);
    spin();

    // Clicking a row selects the cable in the document
    clickRow(w, rowOfCable(w, cable));
    CHECK(sel.currentSelection().contains(&cable));
    CHECK(cable.selection.get());

    // and a binding selects its port
    clickRow(w, rowOfPort(w, bound));
    CHECK(sel.currentSelection().contains(&bound));
    CHECK(!sel.currentSelection().contains(&cable));

    // A selection made elsewhere selects the row; the panel delegate forwards
    // the document's selections to its widget
    score::SelectionDispatcher{sel}.select(cable);
    w.setSelection(sel.currentSelection());
    spin();
    REQUIRE(w.selectedRows().size() == 1);
    CHECK(w.rows()[w.selectedRows().front()].cable == &cable);

    // The selection survives a rebuild
    w.refresh();
    REQUIRE(w.selectedRows().size() == 1);
    CHECK(w.rows()[w.selectedRows().front()].cable == &cable);

    // Delete in the panel removes the cable through one command
    const int before = stack.currentIndex();
    w.view().setFocus();
    score::test::keyClick(w.view(), Qt::Key_Delete);
    spin();
    CHECK(dm.cables.find(cid) == dm.cables.end());
    CHECK(rowOfCable(w, cable) == -1);
    CHECK(stack.currentIndex() == before + 1);

    stack.undo();
    spin();
    REQUIRE(dm.cables.find(cid) != dm.cables.end());
    auto& restored = dm.cables.at(cid);
    CHECK(rowOfCable(w, restored) >= 0);
    CHECK(restored.source().try_find(doc->context()) == a.outlets().front());

    // A cable and a binding together: one undo step for both
    const int cableRow = rowOfCable(w, restored);
    const int portRow = rowOfPort(w, bound);
    REQUIRE(cableRow >= 0);
    REQUIRE(portRow >= 0);
    const int before2 = stack.currentIndex();
    w.removeConnections(w.rowsAt({cableRow, portRow}));
    spin();
    CHECK(stack.currentIndex() == before2 + 1);
    CHECK(dm.cables.find(cid) == dm.cables.end());
    CHECK(!bound.address().address.isSet());
    CHECK(rowOfPort(w, bound) == -1);

    stack.undo();
    spin();
    CHECK(dm.cables.find(cid) != dm.cables.end());
    CHECK(bound.address().address == State::Address{"nodevice", {"x"}});
    CHECK(rowOfPort(w, bound) >= 0);
  });
}

TEST_CASE(
    "Connections panel: selecting reveals the interval of the connection",
    "[integration][gui][dataflow]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& dm
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate());
    auto& base = dm.baseInterval();
    auto presenter
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
            *doc);
    REQUIRE(presenter != nullptr);

    Scenario::ProcessModel* scenario{};
    for(auto& p : base.processes)
      if((scenario = qobject_cast<Scenario::ProcessModel*>(&p)))
        break;
    REQUIRE(scenario != nullptr);

    Scenario::IntervalModel* box{};
    {
      Scenario::Command::Macro m{
          new Scenario::Command::DropProcessInIntervalMacro, doc->context()};
      box = &m.createBox(*scenario, TimeVal::fromMsecs(1000), TimeVal::fromMsecs(3000), 0.2);
      m.commit();
    }
    REQUIRE(box != nullptr);

    auto& outer = addMerger(*doc, base);
    auto& inner = addMerger(*doc, *box);
    CommandDispatcher<> disp{doc->context().commandStack};
    const auto cid = getStrongId(dm.cables);
    disp.submit<Dataflow::CreateCable>(
        dm, cid, Process::CableType::ImmediateGlutton, *inner.outlets().front(),
        *outer.inlets().front());

    const auto innerCable = getStrongId(dm.cables);
    auto& inner2 = addMerger(*doc, *box);
    disp.submit<Dataflow::CreateCable>(
        dm, innerCable, Process::CableType::ImmediateGlutton,
        *inner.outlets().front(), *inner2.inlets().front());

    ConnectionsWidget w;
    w.resize(900, 400);
    w.setDocument(&doc->context());
    w.show();
    spin();

    // Both ends inside the displayed interval: nothing moves
    presenter->setDisplayedInterval(box);
    spin();
    clickRow(w, rowOfCable(w, dm.cables.at(innerCable)));
    CHECK(&presenter->displayedInterval() == box);

    // One end outside: the interval enclosing both is displayed
    clickRow(w, rowOfCable(w, dm.cables.at(cid)));
    CHECK(&presenter->displayedInterval() == &base);
    CHECK(doc->context().selectionStack.currentSelection().contains(&dm.cables.at(cid)));
  });
}

namespace
{
Device::ProtocolFactory* oscFactory(const score::GUIApplicationContext& ctx)
{
  for(auto& f : ctx.interfaces<Device::ProtocolFactoryList>())
    if(f.prettyName() == "OSC")
      return &f;
  return nullptr;
}

Device::AddressSettings parameter(const QString& name)
{
  Device::AddressSettings as;
  as.name = name;
  as.value = 0.f;
  as.ioType = ossia::access_mode::BI;
  return as;
}
}

// The explorer's top-level nodes are devices, which a lookup of address nodes
// misreads; a document opened with devices and bound ports crashed the panel.
TEST_CASE(
    "Connections panel: documents with devices", "[integration][gui][dataflow]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& dm
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate());
    auto& devplug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto* fact = oscFactory(ctx);
    REQUIRE(fact != nullptr);

    CommandDispatcher<> disp{doc->context().commandStack};
    for(QString name : {"lights", "synth"})
    {
      auto settings = fact->defaultSettings();
      settings.name = name;
      Device::Node dev{settings, nullptr};
      dev.push_back(Device::Node{parameter("level"), &dev});
      disp.submit(new Explorer::Command::LoadDevice{devplug, std::move(dev)});
    }
    REQUIRE(devplug.rootNode().childCount() >= 2);

    auto& p = addMerger(*doc, dm.baseInterval());
    p.metadata().setName("Bound merger");
    disp.submit<Process::ChangePortAddress>(
        *p.inlets()[0], State::AddressAccessor{State::Address{"synth", {"level"}}});
    disp.submit<Process::ChangePortAddress>(
        *p.inlets()[1], State::AddressAccessor{State::Address{"synth", {"nothing"}}});
    disp.submit<Process::ChangePortAddress>(
        *p.inlets()[2], State::AddressAccessor{State::Address{"lights", {}}});

    ConnectionsWidget w;
    w.resize(900, 400);
    w.show();

    // The document is handed to the panel while it is being set up, as the
    // document manager does when opening a file or a template
    score::Document* loaded{};
    {
      auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
      const QByteArray bytes = doc->saveAsByteArray();
      QObject::connect(
          &ctx.docManager, &score::DocumentManager::documentChanged, &w,
          [&](score::Document* d) { w.setDocument(d ? &d->context() : nullptr); });
      loaded = ctx.docManager.loadDocument(
          ctx, "with-devices", bytes, DataStream::type(), *delegates.begin());
      REQUIRE(loaded != nullptr);
      CHECK(w.refreshPending());
    }
    spin();

    auto& loadedModel
        = static_cast<Scenario::ScenarioDocumentModel&>(loaded->model().modelDelegate());
    Process::ProcessModel* lp{};
    for(auto& proc : loadedModel.baseInterval().processes)
      if(proc.metadata().getName() == "Bound merger")
        lp = &proc;
    REQUIRE(lp != nullptr);
    REQUIRE(rowOfPort(w, *lp->inlets()[0]) >= 0);
    CHECK(w.rows()[rowOfPort(w, *lp->inlets()[0])].status == ConnectionRow::Resolved);
    CHECK(w.rows()[rowOfPort(w, *lp->inlets()[1])].status == ConnectionRow::MissingNode);
    CHECK(w.rows()[rowOfPort(w, *lp->inlets()[2])].status == ConnectionRow::Resolved);

    // Removing the device while it is listed; undo brings it back
    auto& loadedDevices = loaded->context().plugin<Explorer::DeviceDocumentPlugin>();
    const Device::Node* synth{};
    for(auto& n : loadedDevices.rootNode())
      if(n.is<Device::DeviceSettings>()
         && n.get<Device::DeviceSettings>().name == "synth")
        synth = &n;
    REQUIRE(synth != nullptr);
    CommandDispatcher<> ldisp{loaded->context().commandStack};
    ldisp.submit(new Explorer::Command::Remove{loadedDevices, *synth});
    spin();
    CHECK(w.rows()[rowOfPort(w, *lp->inlets()[0])].status == ConnectionRow::MissingDevice);
    loaded->commandStack().undo();
    spin();
    CHECK(w.rows()[rowOfPort(w, *lp->inlets()[0])].status == ConnectionRow::Resolved);

    // Switching back and forth between the documents
    ctx.docManager.setCurrentDocument(ctx, doc);
    spin();
    CHECK(rowOfPort(w, *p.inlets()[0]) >= 0);
    ctx.docManager.setCurrentDocument(ctx, loaded);
    spin();
    CHECK(rowOfPort(w, *lp->inlets()[0]) >= 0);

    // Closing the listed document, the panel told before or not at all
    QObject::disconnect(&ctx.docManager, nullptr, &w, nullptr);
    ctx.docManager.forceCloseDocument(ctx, *loaded);
    score::test::flush_deferred_deletes();
    spin();
    w.refresh();
    CHECK(w.rows().empty());
    w.setDocument(&doc->context());
    spin();
    CHECK(rowOfPort(w, *p.inlets()[0]) >= 0);
    w.setDocument(nullptr);
  });
}
