// A parameter moved in a VST3 plug-in's editor while nothing plays is in the
// document saved then: its processor, whose state is what gets saved, only
// learns values from process(), but the control has it. Made with
// vst3-fixtures/EditedGain, a gain written with the SDK's classes.

#include <Process/Dataflow/Port.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Vst3/ApplicationPlugin.hpp>
#include <Vst3/EditHandler.hpp>
#include <Vst3/EffectModel.hpp>

#include <core/document/Document.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QCoreApplication>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <pluginterfaces/vst/ivstaudioprocessor.h>

namespace
{
const QString vst3_uuid = QStringLiteral("4cc30435-3237-4e94-a79e-1c2bd6b724a1");

QString processor_uid(vst3::ApplicationPlugin& plug)
{
  auto mdl = plug.getModule(SCORE_VST3_EDITED_GAIN);
  REQUIRE(mdl);
  vst3::AvailablePlugin p;
  p.path = QStringLiteral(SCORE_VST3_EDITED_GAIN);
  p.name = QString::fromStdString(mdl->getName());
  QString uid;
  for(const auto& cls : mdl->getFactory().classInfos())
    if(cls.category() == kVstAudioEffectClass)
    {
      p.classInfo.push_back(cls);
      uid = QString::fromStdString(cls.ID().toString());
    }
  p.isValid = true;
  plug.vst_infos = {p};
  return uid;
}

Process::ControlInlet* gain_control(Process::ProcessModel& proc)
{
  for(auto* in : proc.inlets())
    if(auto* c = qobject_cast<Process::ControlInlet*>(in); c && c->name() == "Gain")
      return c;
  return nullptr;
}
}

TEST_CASE(
    "VST3: a parameter moved in the editor while nothing plays is saved",
    "[vst3][state]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = ctx.applicationPlugin<vst3::ApplicationPlugin>();
    const QString uid = processor_uid(plug);
    REQUIRE(!uid.isEmpty());

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(*doc, vst3_uuid, uid);
    REQUIRE(proc);
    // vst3::Model is not exported: no qobject_cast
    auto& model = static_cast<vst3::Model&>(*proc);
    REQUIRE(model.fx.controller);
    REQUIRE(model.fx.componentHandler);
    auto* control = gain_control(model);
    REQUIRE(control);

    // What the SDK's editors do as the knob turns
    model.fx.controller->setParamNormalized(0, 0.8);
    auto* handler
        = static_cast<Steinberg::Vst::IComponentHandler*>(model.fx.componentHandler);
    handler->beginEdit(0);
    handler->performEdit(0, 0.8);
    handler->endEdit(0);
    QCoreApplication::processEvents();
    REQUIRE(ossia::convert<double>(control->value()) == Catch::Approx(0.8));

    const auto id = proc->id();
    auto* doc2 = score::test::reload_via_json(ctx, *doc);
    REQUIRE(doc2);
    auto& itv = score::test::base_interval(*doc2);
    auto it = itv.processes.find(id);
    REQUIRE(it != itv.processes.end());
    auto& loaded = static_cast<vst3::Model&>(*it);
    REQUIRE(loaded.fx.controller);
    auto* loaded_control = gain_control(loaded);
    REQUIRE(loaded_control);
    CHECK(ossia::convert<double>(loaded_control->value()) == Catch::Approx(0.8));
    CHECK(loaded.fx.controller->getParamNormalized(0) == Catch::Approx(0.8));
    // The processor, which only learns it by playing, is given it then
    CHECK(loaded.fx.unsentParameters.contains(0));
  });
}
