// App test: the audio engine across documents being opened, switched and closed.
//
// The engine used to be stopped whenever any document closed and restarted
// only when the current document changed. Closing a document that is not the
// current one changes nothing, so the engine stayed stopped - audio dead, the
// toolbar still showing it on - until a tab switch or a manual restart; and
// closing the current one recreated the audio driver each time. The engine
// now lives as long as a document does: closing or switching documents only
// rebinds the audio device of the current document to it.
//
// Every case boots its own app: several run_in_app() in one process do not
// survive on Windows (see reference_score_test_multicase_crash), run them with
// `-c` / the test name.

#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/Settings/Model.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/audio/audio_engine.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/value/format_value.hpp>

#include <Device/Node/DeviceNode.hpp>

#include <QApplication>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

namespace
{
Audio::ApplicationPlugin& audioPlugin(const score::GUIApplicationContext& ctx)
{
  return ctx.guiApplicationPlugin<Audio::ApplicationPlugin>();
}

bool audioDeviceConnected(score::Document& doc)
{
  auto dev = doc.context().plugin<Explorer::DeviceDocumentPlugin>().list().audioDevice();
  return dev && dev->connected();
}

void spin()
{
  QApplication::processEvents();
  QApplication::processEvents();
}
}

TEST_CASE("Closing a background document keeps the audio engine running", "[audio][documents]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& audio = audioPlugin(ctx);
    auto a = score::test::new_document(ctx);
    auto b = score::test::new_document(ctx);
    REQUIRE(ctx.docManager.currentDocument() == b);
    REQUIRE(audio.audio);
    const auto engine = audio.audio;

    ctx.docManager.forceCloseDocument(ctx, *a);
    spin();

    // Still the current document, nothing changed for it
    CHECK(ctx.docManager.currentDocument() == b);
    REQUIRE(audio.audio);
    CHECK(audio.audio == engine);
    CHECK(audioDeviceConnected(*b));
  });
}

TEST_CASE("Closing the current document keeps the engine for the remaining one", "[audio][documents]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& audio = audioPlugin(ctx);
    auto a = score::test::new_document(ctx);
    auto b = score::test::new_document(ctx);
    REQUIRE(ctx.docManager.currentDocument() == b);
    REQUIRE(audio.audio);
    const auto engine = audio.audio;

    ctx.docManager.forceCloseDocument(ctx, *b);
    spin();

    CHECK(ctx.docManager.currentDocument() == a);
    REQUIRE(audio.audio);
    CHECK(audio.audio == engine);
    CHECK(audioDeviceConnected(*a));
  });
}

TEST_CASE("Switching documents keeps the engine and rebinds the audio device", "[audio][documents]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& audio = audioPlugin(ctx);
    auto a = score::test::new_document(ctx);
    auto b = score::test::new_document(ctx);
    REQUIRE(audio.audio);
    const auto engine = audio.audio;

    ctx.docManager.setCurrentDocument(ctx, a);
    spin();
    CHECK(audio.audio == engine);
    CHECK(audioDeviceConnected(*a));

    ctx.docManager.setCurrentDocument(ctx, b);
    spin();
    CHECK(audio.audio == engine);
    CHECK(audioDeviceConnected(*b));
  });
}

TEST_CASE("Closing the last document stops the engine, opening one starts it", "[audio][documents]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& audio = audioPlugin(ctx);
    auto a = score::test::new_document(ctx);
    REQUIRE(audio.audio);

    ctx.docManager.forceCloseDocument(ctx, *a);
    spin();
    CHECK(!audio.audio);

    auto b = score::test::new_document(ctx);
    spin();
    REQUIRE(audio.audio);
    CHECK(audioDeviceConnected(*b));
  });
}

TEST_CASE("Changing the audio settings restarts the engine", "[audio][documents]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& audio = audioPlugin(ctx);
    auto a = score::test::new_document(ctx);
    REQUIRE(audio.audio);
    const auto engine = audio.audio;

    auto& set = ctx.settings<Audio::Settings::Model>();
    set.setBufferSize(set.getBufferSize() == 256 ? 512 : 256);
    set.changed();
    spin();

    REQUIRE(audio.audio);
    CHECK(audio.audio != engine);
    CHECK(audioDeviceConnected(*a));
  });
}

namespace
{
ossia::net::parameter_base* masterParameter(score::Document& doc)
{
  auto dev = doc.context().plugin<Explorer::DeviceDocumentPlugin>().list().audioDevice();
  if(!dev || !dev->getDevice())
    return nullptr;
  auto node = ossia::net::find_node(dev->getDevice()->get_root_node(), "/out/main");
  return node ? node->get_parameter() : nullptr;
}
}

TEST_CASE("A saved master volume reaches the engine, and stays", "[audio][documents][volume]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    spin();
    REQUIRE(audioDeviceConnected(*doc));
    auto* dev = doc->context().plugin<Explorer::DeviceDocumentPlugin>().list().audioDevice();
    REQUIRE(dev);

    // What loading a document does with the /out/main node it saved.
    Device::FullAddressSettings saved;
    saved.address = State::Address{dev->settings().name, {"out", "main"}};
    saved.value = 0.25f;
    dev->addAddress(saved);
    auto* p = masterParameter(*doc);
    REQUIRE(p);
    CHECK(ossia::convert<float>(p->value()) == Catch::Approx(0.25f));

    // The engine rebinds the device (document switch, audio restart): a new
    // tree, with the same volume.
    p->push_value(0.5f);
    dev->reconnect();
    spin();
    auto* q = masterParameter(*doc);
    REQUIRE(q);
    CHECK(ossia::convert<float>(q->value()) == Catch::Approx(0.5f));

    // A document being loaded: the address is added before the device has a
    // tree, and the first binding builds that tree with the saved value.
    dev->disconnect();
    saved.value = 0.75f;
    dev->addAddress(saved);
    dev->reconnect();
    spin();
    auto* r = masterParameter(*doc);
    REQUIRE(r);
    CHECK(ossia::convert<float>(r->value()) == Catch::Approx(0.75f));
  });
}
