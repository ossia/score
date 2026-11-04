// A document with a Window device in Background mode closes cleanly.
//
// The background device draws into the document's scenario view and listens
// to that view's signals (size, hover, tablet, keys). Taking its renderer off
// the view changes the view's background mode, which can resize the view: a
// sizeChanged delivered while the device is being destroyed reaches a size
// parameter whose device is half gone. Closing such a document crashed in
// ~background_device every time.
//
// Covered: a device added to a fresh document, and the same document saved
// and loaded back (the path a user opening a .score takes), each closed with
// and without a shader rendering into it, and the view resized while the
// device is alive so the size connection is known to be live.

#include "WindowedOutputCommon.hpp"

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <Gfx/GfxParameter.hpp>
#include <Gfx/WindowDevice.hpp>

#include <JS/Qml/EditContext.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/DocumentView.hpp>

#include <ossia/network/base/node_functions.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace Gfx;
using namespace score::test;
using namespace score::test::gfx;

namespace
{
WindowDevice* add_background_device(const score::DocumentContext& doc, QString name)
{
  auto& plug = doc.plugin<Explorer::DeviceDocumentPlugin>();
  Device::DeviceSettings set;
  set.name = name;
  set.protocol = WindowProtocolFactory::static_concreteKey();
  WindowSettings ws;
  ws.mode = WindowMode::Background;
  ws.inputWidth = 1920;
  ws.inputHeight = 1080;
  set.deviceSpecificSettings = QVariant::fromValue(ws);

  CommandDispatcher<> disp{doc.commandStack};
  disp.submit(new Explorer::Command::LoadDevice{plug, std::move(set)});

  return dynamic_cast<WindowDevice*>(plug.list().findDevice(name));
}

Scenario::ScenarioDocumentView* scenario_view(score::Document& doc)
{
  if(auto* v = doc.view())
    return qobject_cast<Scenario::ScenarioDocumentView*>(&v->viewDelegate());
  return nullptr;
}

ossia::net::parameter_base* size_parameter(WindowDevice& dev)
{
  if(auto* d = dev.getDevice())
    if(auto* n = ossia::net::find_node(d->get_root_node(), "/size"))
      return n->get_parameter();
  return nullptr;
}

struct Outcome
{
  bool skipped{};
  std::string skipReason;
  bool sizeFollowedView{};
};
}

TEST_CASE(
    "a document with a Background window device closes without crashing",
    "[gfx][window][device][lifecycle][background]")
{
  const bool reload = GENERATE(false, true);
  const bool render = GENERATE(false, true);
  CAPTURE(reload, render);

  Outcome o;
  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    if(!doc)
    {
      o = {true, "no document delegate"};
      return;
    }
    if(!scenario_view(*doc))
    {
      o = {true, "the document has no scenario view to draw a background in"};
      return;
    }

    auto* dev = add_background_device(doc->context(), QStringLiteral("Win"));
    REQUIRE(dev);
    REQUIRE(dev->getDevice());

    if(reload)
    {
      auto* reloaded = score::test::reload_via_json(ctx, *doc);
      REQUIRE(reloaded);
      ctx.docManager.forceCloseDocument(ctx, *doc);
      QApplication::processEvents();
      doc = reloaded;
      auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
      dev = dynamic_cast<WindowDevice*>(plug.list().findDevice(QStringLiteral("Win")));
      REQUIRE(dev);
      REQUIRE(dev->getDevice());
    }

    auto* view = scenario_view(*doc);
    REQUIRE(view);

    if(render)
    {
      JS::EditJsContext api;
      auto* itv = api.rootInterval();
      REQUIRE(itv);
      auto* proc = api.createProcess(
          itv, QStringLiteral("74ca45ff-92c9-44a0-8f1a-754dea05ee1b"),
          QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-solid-color.fs"));
      REQUIRE(proc);
      auto* out = api.outlet(proc, 0);
      REQUIRE(out);
      api.setAddress(out, QStringLiteral("Win:/"));
      api.play();
      pump_for(500);
    }

    // The size connection is live: a resize of the view reaches the device.
    auto& gv = view->view();
    if(auto* w = gv.window())
    {
      w->show();
      w->resize(900, 700);
    }
    pump_for(100);
    if(auto* p = size_parameter(*dev))
    {
      const auto before = p->value();
      gv.resize(gv.width() + 37, gv.height() + 23);
      pump_for(100);
      o.sizeFollowedView = p->value() != before;
    }

    if(render)
    {
      JS::EditJsContext api;
      api.stop();
      pump_for(200);
    }

    ctx.docManager.forceCloseDocument(ctx, *doc);
    pump_for(200);
  });

  if(o.skipped)
    SKIP(o.skipReason);
  CHECK(o.sizeFollowedView);
}
