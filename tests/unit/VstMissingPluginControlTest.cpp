// A document can reference a VST that is not installed on this machine: the
// process loads with its saved control ports and no plug-in behind them. Its
// controls still get a slider, which shows and edits the saved value. The
// factory returned no item there, which PortFactory::makeFullItem treats as a
// bug (SCORE_SOFT_ASSERT): a debug build stopped on every such score
// (score-corpus 2026/piranesi).
//
// Through the factory lists: the plug-in's classes are not exported.

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QImage>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <memory>

namespace
{
// A VST process as a document saves it, from score-corpus 2026/piranesi.score
// without its plug-in state, and with an effect id no plug-in has.
constexpr const char* k_saved_vst = R"_({"uuid": "be8e6bd3-75f2-4102-8895-8a4eb4ea545a", "ObjectName": "vst", "id": 24, "Metadata": {"ScriptingName": "Glitch2", "Comment": "", "Color": "Transparent1", "Label": "Glitch2", "Touched": true}, "Duration": 10584000000, "Height": 300.0, "StartOffset": 0, "LoopDuration": 10584000000, "Loops": false, "Inlets": [{"uuid": "a1574bb0-cbd4-4c7d-9417-0c25cfd1187b", "ObjectName": "Inlet", "id": 0, "Hidden": false, "Custom": "Audio In", "Exposed": "audio in"}, {"uuid": "e523bc44-8599-4a04-94c1-04ce0d1a692a", "ObjectName": "Inlet", "id": 1, "Hidden": true, "Custom": "MST:Mix", "Exposed": "mst:mix", "FxNum": 1, "Value": 0.25}], "Outlets": [{"uuid": "a1d97535-18ac-444a-8417-0cbc1692d897", "ObjectName": "Outlet", "id": 0, "Hidden": false, "Custom": "Audio Out", "Exposed": "audio out", "GainInlet": {"uuid": "9a13fb32-269a-47bf-99a9-930188c1f19c", "ObjectName": "Inlet", "id": 10000, "Hidden": false, "Custom": "Gain", "Exposed": "gain", "Value": {}, "Init": {}, "Domain": {"Float": {"Min": 0.0, "Max": 1.0}}}, "PanInlet": {"uuid": "9a13fb32-269a-47bf-99a9-930188c1f19c", "ObjectName": "Inlet", "id": 10001, "Hidden": false, "Custom": "Pan", "Exposed": "pan", "Value": {}, "Init": {}, "Domain": {}}, "Gain": 1.0, "Pan": [1.0, 1.0], "Propagate": false}], "EffectId": 1})_";
}

TEST_CASE("a control of a VST that is not installed gets a slider", "[vst][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& dctx = doc->context();

    const auto json = readJson(QByteArray{k_saved_vst});
    JSONObject::Deserializer des{json};
    std::unique_ptr<Process::ProcessModel> proc{deserialize_interface(
        ctx.interfaces<Process::ProcessFactoryList>(), des, dctx, &doc->model())};
    REQUIRE(proc != nullptr);

    Process::ControlInlet* control{};
    for(auto* inlet : proc->inlets())
      if(auto* c = qobject_cast<Process::ControlInlet*>(inlet))
        control = c;
    REQUIRE(control != nullptr);

    auto* factory = ctx.interfaces<Process::PortFactoryList>().get(control->concreteKey());
    REQUIRE(factory != nullptr);

    QObject context;
    std::unique_ptr<QGraphicsItem> item{
        factory->makeControlItem(*control, dctx, nullptr, &context)};
    REQUIRE(item != nullptr);

    // It draws something: the saved value, with no plug-in to format it.
    const QRectF r = item->boundingRect();
    REQUIRE(r.width() > 0);
    REQUIRE(r.height() > 0);
    QImage img{r.size().toSize(), QImage::Format_ARGB32};
    img.fill(Qt::transparent);
    {
      QPainter p{&img};
      p.translate(-r.topLeft());
      QStyleOptionGraphicsItem opt;
      item->paint(&p, &opt, nullptr);
    }
    bool drawn = false;
    for(int y = 0; y < img.height() && !drawn; y++)
      for(int x = 0; x < img.width() && !drawn; x++)
        drawn = qAlpha(img.pixel(x, y)) != 0;
    CHECK(drawn);
  });
}
