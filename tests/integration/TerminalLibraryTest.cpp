// The library panel is one; documents are many. What is available to a score is
// a property of the document -- one that runs on another machine can only use
// that machine's processes -- so the panel has to follow whichever document is
// visible. Needs the GUI stack, since panels do not exist without it, and a
// binary cannot mix the two application fixtures.

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <Library/Panel/LibraryPanelDelegate.hpp>
#include <Library/ProcessWidget.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <score/document/DocumentRole.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/ProbeProtocol.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
QByteArray asJson(score::Document& doc)
{
  JSONObject::Serializer wr{};
  doc.saveAsJson(wr);
  return wr.toByteArray();
}

QByteArray documentWithProbeDevice(const score::GUIApplicationContext& ctx)
{
  auto* doc = score::test::new_document(ctx);
  SCORE_ASSERT(doc);

  auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
  plug.explorer().addDevice(score::test::probe_device_node(QStringLiteral("probe")));
  return asJson(*doc);
}

score::Document* reload(
    const score::GUIApplicationContext& ctx, const QByteArray& bytes,
    score::DocumentRole role)
{
  auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
  SCORE_ASSERT(!delegates.empty());
  auto* doc = ctx.docManager.loadDocument(
      ctx, QStringLiteral("terminal"), bytes, JSONObject::type(), *delegates.begin(),
      role);
  QApplication::processEvents();
  return doc;
}
}

TEST_CASE("The library follows the document that is visible", "[terminal]")
{
  // No asynchronous file scan. rescan() posts one to the task pool, and that
  // thread outlives the application: on the way out it calls back into library
  // interfaces whose plug-ins have been unloaded. A real crash, reachable from
  // Settings > rescan library and from quitting during the startup scan, but
  // not this test's subject -- which is whether rescan is called at all, and
  // the factory-derived entries it adds are there before the scan starts.
  qputenv("SCORE_DISABLE_LIBRARY", "1");

  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    // Panels exist only with the GUI stack, so this case needs run_in_gui_app;
    // finding none would make every assertion below vacuous.
    auto* panel = ctx.findPanel<Library::ProcessPanel>();
    REQUIRE(panel);

    auto& model = panel->processWidget().processModel();
    score::test::register_probe_protocol(ctx);
    const auto bytes = documentWithProbeDevice(ctx);

    auto* local = reload(ctx, bytes, score::DocumentRole::Local);
    REQUIRE(local);
    ctx.docManager.setCurrentDocument(ctx, local);
    QApplication::processEvents();

    // A document that runs here lists this build's processes.
    REQUIRE(model.rootNode().childCount() > 0);

    // Emptied as whatever mirrors another machine would leave it.
    model.beginResetModel();
    model.rootNode().erase(model.rootNode().begin(), model.rootNode().end());
    model.endResetModel();
    REQUIRE(model.rootNode().childCount() == 0);

    // Showing a terminal must not put this build's processes back: the score
    // runs elsewhere and none of them can run for it.
    auto* terminal = reload(ctx, bytes, score::DocumentRole::Terminal);
    REQUIRE(terminal);
    ctx.docManager.setCurrentDocument(ctx, terminal);
    QApplication::processEvents();
    CHECK(model.rootNode().childCount() == 0);

    // Coming back to one that does run here restores them, which is what makes
    // the panel usable with more than one document open.
    ctx.docManager.setCurrentDocument(ctx, local);
    QApplication::processEvents();
    CHECK(model.rootNode().childCount() > 0);
  });
}
