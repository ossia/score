// Conformance sweep over the Javascript presets shipped in the user library.
//
// Every .qml under <library>/Presets/Javascript is loaded through the real QML
// engine, its ports are collected the way JS::js_node::setupComponent collects
// them, and it is ticked with values pushed into its inlets exactly the way
// JS::js_node::run pushes them — setValue() for "the value of this tick" plus
// addValue() for the timestamped message list a script reads through
// `inlet.values`.
//
// This is a conformance test rather than a behaviour test. Per preset it
// asserts that:
//  * the script parses and produces a JS::Script,
//  * every port it declares carries a name (objectName), which is what shows
//    up in the process inspector,
//  * ticking it never raises a JS exception — including the first tick, where
//    controls have not been assigned yet and read back as an invalid QVariant,
//  * nothing it emits is NaN or infinite.
//
// It then pins the multi-value contract: a preset fed three values in a single
// tick must see all three in `inlet.values`, and a pass-through preset must
// emit all three rather than only the last.

#include <score_test/App.hpp>

#include <Library/LibrarySettings.hpp>

#include <JS/Executor/CPUNode.hpp>
#include <JS/Qml/QmlObjects.hpp>
#include <JS/Qml/Utils.hpp>
#include <JS/Qml/ValueTypes.Qt6.hpp>

#include <score/application/ApplicationContext.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/token_request.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <QDirIterator>
#include <QStandardPaths>
#include <QFileInfo>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
// ---------------------------------------------------------------------------
// Harness
// ---------------------------------------------------------------------------

//! The ports of a script, classified in the order — and by the same rules —
//! that JS::js_node::setupComponent uses. Impulse derives from ControlInlet so
//! it has to be tested for first, exactly as the executor does.
struct script_ports
{
  std::vector<JS::Impulse*> impulses;
  std::vector<JS::ControlInlet*> controls;
  std::vector<JS::ValueInlet*> value_inlets;
  std::vector<JS::ValueOutlet*> value_outlets;
  int inlets{};
  int outlets{};
  bool unnamed_port{};
};

script_ports collect_ports(JS::Script& script)
{
  script_ports p;
  for(auto* n : script.children())
  {
    const auto named = [&](QObject* o) {
      if(o->objectName().isEmpty())
        p.unnamed_port = true;
    };

    if(auto* imp = qobject_cast<JS::Impulse*>(n))
    {
      p.impulses.push_back(imp);
      p.controls.push_back(imp);
      named(imp);
      p.inlets++;
    }
    else if(auto* ctl = qobject_cast<JS::ControlInlet*>(n))
    {
      p.controls.push_back(ctl);
      named(ctl);
      p.inlets++;
    }
    else if(auto* vin = qobject_cast<JS::ValueInlet*>(n))
    {
      p.value_inlets.push_back(vin);
      named(vin);
      p.inlets++;
    }
    else if(auto* vout = qobject_cast<JS::ValueOutlet*>(n))
    {
      p.value_outlets.push_back(vout);
      named(vout);
      p.outlets++;
    }
    else if(qobject_cast<JS::AudioInlet*>(n) || qobject_cast<JS::MidiInlet*>(n))
    {
      named(n);
      p.inlets++;
    }
    else if(qobject_cast<JS::AudioOutlet*>(n) || qobject_cast<JS::MidiOutlet*>(n))
    {
      named(n);
      p.outlets++;
    }
  }
  return p;
}

//! Gives a control the value its QML declaration asks for. The executor does
//! this from the model port; here the declaration is the only source of truth.
void apply_declared_default(JS::ControlInlet& c)
{
  const QMetaObject* mo = c.metaObject();
  if(mo->indexOfProperty("choices") >= 0)
  {
    const auto choices = c.property("choices").toStringList();
    const int idx = c.property("index").toInt();
    if(idx >= 0 && idx < choices.size())
      c.setValue(choices[idx]);
  }
  else if(mo->indexOfProperty("text") >= 0)
  {
    c.setValue(c.property("text"));
  }
  else if(mo->indexOfProperty("init") >= 0)
  {
    c.setValue(c.property("init"));
  }
  else if(mo->indexOfProperty("checked") >= 0)
  {
    c.setValue(c.property("checked"));
  }
}

//! One tick's worth of input for a value inlet, pushed the way CPUNode does:
//! setValue() for each message, addValue() to build up `inlet.values`.
void push_values(JS::ValueInlet& inlet, const QVariantList& vals, int64_t first_ts)
{
  inlet.clear();
  if(vals.empty())
  {
    inlet.setValue(QVariant{});
    return;
  }
  for(int i = 0; i < vals.size(); i++)
  {
    QVariant v = vals[i];
    inlet.setValue(v);
    inlet.addValue(QVariant::fromValue(
        JS::InValueMessage{(double)(first_ts + i * 8), std::move(v)}));
  }
}

//! Stimulus for tick n: scalars, lists and vectors in turn, so a preset gets
//! hit with every shape a cable can actually carry.
QVariantList stimulus(int tick, int inlet_index)
{
  const double phase = 0.05 * (tick + 1) + 0.13 * inlet_index;
  switch((tick + inlet_index) % 4)
  {
    case 0:
      return {QVariant(std::fmod(phase, 1.0))};
    case 1:
      return {QVariant(QVariantList{0.25, 0.5, std::fmod(phase, 1.0)})};
    case 2:
      // Two messages in one tick: the case `inlet.values` exists for.
      return {QVariant(std::fmod(phase, 1.0)), QVariant(std::fmod(phase + 0.4, 1.0))};
    default:
      return {QVariant(QVariantList{
          QVariantList{0.1, 0.2}, QVariantList{0.3, 0.4}, QVariantList{0.5, 0.6}})};
  }
}

void check_finite(const ossia::value& v, const std::string& where, bool strict)
{
  const auto num = [&](float f, int idx) {
    if(std::isfinite(f))
      return;
    if(strict)
    {
      INFO(where << "[" << idx << "] = " << f);
      CHECK(std::isfinite(f));
    }
    else
    {
      WARN(where << "[" << idx << "] = " << f);
    }
  };
  switch(v.get_type())
  {
    case ossia::val_type::FLOAT:
      num(*v.target<float>(), 0);
      break;
    case ossia::val_type::VEC2F: {
      auto& a = *v.target<ossia::vec2f>();
      for(int i = 0; i < 2; i++)
        num(a[i], i);
      break;
    }
    case ossia::val_type::VEC3F: {
      auto& a = *v.target<ossia::vec3f>();
      for(int i = 0; i < 3; i++)
        num(a[i], i);
      break;
    }
    case ossia::val_type::VEC4F: {
      auto& a = *v.target<ossia::vec4f>();
      for(int i = 0; i < 4; i++)
        num(a[i], i);
      break;
    }
    case ossia::val_type::LIST: {
      auto& l = *v.target<std::vector<ossia::value>>();
      for(std::size_t i = 0; i < l.size(); i++)
        check_finite(l[i], where + "[" + std::to_string(i) + "]", strict);
      break;
    }
    default:
      // int / bool / impulse / string / map: nothing that can be NaN.
      break;
  }
}

constexpr int64_t flicks_per_sample(int rate)
{
  return 705600000 / rate;
}

ossia::token_request make_token(int tick, int rate, int buffer)
{
  const int64_t per_buffer = flicks_per_sample(rate) * buffer;
  ossia::token_request tk;
  tk.prev_date = ossia::time_value{per_buffer * tick};
  tk.date = ossia::time_value{per_buffer * (tick + 1)};
  tk.parent_duration = ossia::time_value{per_buffer * 256};
  tk.speed = 1.;
  tk.tempo = 120.;
  tk.signature = ossia::time_signature{4, 4};
  tk.start_sample = 0;
  tk.length_sample = buffer;
  return tk;
}

//! Loads one preset. Returns nullptr and records why on failure.
JS::Script* load_script(
    QQmlEngine& engine, QQmlContext* ctx, const QString& path, bool strict)
{
  auto* comp = new QQmlComponent{&engine, QUrl::fromLocalFile(path), &engine};
  if(!comp->errors().empty())
  {
    for(const auto& e : comp->errors())
    {
      if(strict)
      {
        INFO(path.toStdString() << ": " << e.toString().toStdString());
        FAIL_CHECK("QML error");
      }
      else
      {
        WARN(path.toStdString() << ": " << e.toString().toStdString());
      }
    }
    return nullptr;
  }
  auto* obj = comp->create(ctx);
  auto* script = qobject_cast<JS::Script*>(obj);
  if(!script)
    delete obj;
  return script;
}

//! Drives one preset through `ticks` ticks and reports whether it stayed quiet.
//! `fire_impulses` bangs every Impulse port on every fourth tick so counters,
//! metros and random pickers actually do something.
struct run_result
{
  bool js_error{};
  QString first_error;
  int emitted{};
};

run_result
tick_script(
    QQmlEngine& engine, JS::Script& script, script_ports& ports, int ticks, bool strict)
{
  run_result res;

  ossia::execution_state st;
  st.sampleRate = 48000;
  st.bufferSize = 64;
  const ossia::exec_state_facade facade{&st};

  for(int t = 0; t < ticks; t++)
  {
    for(std::size_t i = 0; i < ports.value_inlets.size(); i++)
      push_values(*ports.value_inlets[i], stimulus(t, (int)i), 0);

    if(t % 4 == 3)
      for(auto* imp : ports.impulses)
        imp->impulse();

    for(auto* out : ports.value_outlets)
      out->clear();

    const auto tk = make_token(t, st.sampleRate, st.bufferSize);
    QJSValueList args{
        engine.toScriptValue(JS::TokenRequestValueType{tk}),
        engine.toScriptValue(JS::ExecutionStateValueType{facade})};

    auto r = const_cast<QJSValue&>(script.tick()).call(args);
    if(r.isError() && !res.js_error)
    {
      res.js_error = true;
      res.first_error = QStringLiteral("line %1: %2")
                            .arg(r.property("lineNumber").toInt())
                            .arg(r.toString());
    }

    for(auto* out : ports.value_outlets)
    {
      const QJSValue& v = out->value();
      if(!v.isNull() && !v.isError() && !v.isUndefined())
      {
        check_finite(
            ossia::qt::value_from_js(v), out->objectName().toStdString() + ".value", strict);
        res.emitted++;
      }
      for(auto& m : out->values)
      {
        check_finite(
            ossia::qt::value_from_js(QJSValue{m.value}),
            out->objectName().toStdString() + ".values", strict);
        res.emitted++;
      }
    }
  }
  return res;
}

//! Where the Javascript presets are.
//!
//! SCORE_JS_PRESETS_DIR wins, so CI can point this at a checkout of
//! score-user-library. Otherwise we look at the configured library root — but
//! that setting is shared with the other tests, one of which redirects it to a
//! scratch folder — and then at the place the user library actually installs
//! itself. Empty means "not installed": the test warns and stops.
QString find_presets_root(const Library::Settings::Model& lib)
{
  if(const auto env = qEnvironmentVariable("SCORE_JS_PRESETS_DIR"); !env.isEmpty())
    return QFileInfo{env}.isDir() ? env : QString{};

  const QStringList candidates{
      lib.getDefaultLibraryPath() + "/Presets/Javascript",
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
          + "/ossia/score/packages/default/Presets/Javascript"};

  for(const auto& c : candidates)
    if(QFileInfo{c}.isDir())
      return c;
  return {};
}

QStringList find_presets(const QString& root)
{
  QStringList out;
  QDirIterator it{root, QStringList{"*.qml"}, QDir::Files, QDirIterator::Subdirectories};
  while(it.hasNext())
    out.push_back(it.next());
  out.sort();
  return out;
}
}

TEST_CASE("Javascript presets load, expose named ports and tick", "[integration][js][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    const auto& lib = ctx.settings<Library::Settings::Model>();
    const QString root = find_presets_root(lib);
    if(root.isEmpty())
    {
      WARN("No Javascript presets found; set SCORE_JS_PRESETS_DIR to run this test");
      return;
    }

    const auto presets = find_presets(root);
    INFO("library: " << root.toStdString());
    REQUIRE(!presets.empty());

    QQmlEngine engine;
    for(const auto& p : lib.getIncludePaths())
      engine.addImportPath(p);
    auto* utils = new JS::JsUtils;
    utils->setParent(&engine);
    engine.rootContext()->setContextProperty("Util", utils);

    int loaded = 0;
    for(const QString& file : presets)
    {
      const auto shortName = QDir{root}.relativeFilePath(file);
      INFO("preset: " << shortName.toStdString());

      // The Utils tree is the curated set that holds to the contract below and
      // is expected to stay green. The rest of the library is swept too, but a
      // failure there is reported as a warning: those presets predate this test
      // and several of them legitimately need a file, a device, a QML module or
      // their UI half.
      const bool strict = shortName.startsWith(QStringLiteral("Utils/"));
      const auto expect = [&](bool ok, const char* what) {
        if(strict)
        {
          INFO(what);
          CHECK(ok);
        }
        else if(!ok)
        {
          WARN(shortName.toStdString() << ": " << what);
        }
      };

      auto* context = new QQmlContext{&engine, &engine};
      auto* script = load_script(engine, context, file, strict);
      if(!script)
      {
        // A .qml in this tree that is not a Script (a UI component pulled in by
        // another preset) is not a failure on its own.
        continue;
      }
      loaded++;

      auto ports = collect_ports(*script);
      expect(!ports.unnamed_port, "every port should carry an objectName");
      expect(ports.inlets + ports.outlets > 0, "a preset should declare ports");

      if(!script->tick().isCallable())
      {
        delete script;
        continue;
      }

      // First pass: controls left untouched, the way they are on the very
      // first tick before the model has pushed anything into them.
      auto first = tick_script(engine, *script, ports, 4, strict);
      INFO("with unset controls: " << first.first_error.toStdString());
      expect(!first.js_error, "ticking with unset controls should not throw");

      // Second pass: controls carrying the value their QML declares.
      for(auto* c : ports.controls)
        apply_declared_default(*c);
      auto second = tick_script(engine, *script, ports, 12, strict);
      INFO("with declared controls: " << second.first_error.toStdString());
      expect(!second.js_error, "ticking with declared controls should not throw");

      delete script;
    }

    INFO("loaded " << loaded << " of " << presets.size() << " .qml files");
    CHECK(loaded > 0);
  });
}

TEST_CASE(
    "A ValueInlet reports every value received during a tick",
    "[integration][js][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    QQmlEngine engine;

    // Minimal pass-through written against the documented contract: read every
    // message of the tick from `values`, re-emit each at its own timestamp.
    static const char* src = R"QML(
import Score 1.0
Script {
  ValueInlet { id: input; objectName: "In" }
  ValueOutlet { id: output; objectName: "Out" }
  property int seen: 0
  tick: function(token, state) {
    var vs = input.values;
    seen = (vs === undefined) ? -1 : vs.length;
    for(var i = 0; i < seen; i++)
      output.addValue(vs[i].timestamp, vs[i].value);
  }
}
)QML";

    QQmlComponent comp{&engine};
    comp.setData(src, QUrl{});
    for(const auto& e : comp.errors())
    {
      INFO(e.toString().toStdString());
      FAIL("could not build the probe script");
    }

    auto* script = qobject_cast<JS::Script*>(comp.create(engine.rootContext()));
    REQUIRE(script != nullptr);

    auto ports = collect_ports(*script);
    REQUIRE(ports.value_inlets.size() == 1);
    REQUIRE(ports.value_outlets.size() == 1);

    auto& inlet = *ports.value_inlets[0];
    auto& outlet = *ports.value_outlets[0];

    ossia::execution_state st;
    st.sampleRate = 48000;
    st.bufferSize = 64;
    const ossia::exec_state_facade facade{&st};
    const auto tk = make_token(0, st.sampleRate, st.bufferSize);
    QJSValueList args{
        engine.toScriptValue(JS::TokenRequestValueType{tk}),
        engine.toScriptValue(JS::ExecutionStateValueType{facade})};

    // Three values in one tick, at three different sample offsets.
    outlet.clear();
    push_values(inlet, QVariantList{0.25, 0.5, 0.75}, 0);
    REQUIRE(inlet.values().size() == 3);

    auto r = const_cast<QJSValue&>(script->tick()).call(args);
    INFO(r.toString().toStdString());
    REQUIRE_FALSE(r.isError());

    // The script must have seen all three, not just the last one.
    CHECK(script->property("seen").toInt() == 3);
    CHECK(outlet.values.size() == 3);
    if(outlet.values.size() == 3)
    {
      CHECK(outlet.values[0].timestamp == 0.);
      CHECK(outlet.values[1].timestamp == 8.);
      CHECK(outlet.values[2].timestamp == 16.);
      CHECK(outlet.values[2].value.toNumber() == 0.75);
    }

    // A tick with nothing incoming leaves an empty list and an undefined value.
    outlet.clear();
    push_values(inlet, QVariantList{}, 0);
    r = const_cast<QJSValue&>(script->tick()).call(args);
    REQUIRE_FALSE(r.isError());
    CHECK(script->property("seen").toInt() == 0);
    CHECK(outlet.values.empty());

    delete script;
  });
}

TEST_CASE(
    "The Javascript executor hands a script every value of a tick",
    "[integration][js][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    // js_node asserts on the thread kind; it only warns, but the assertion
    // underneath requires the current thread to have a kind at all.
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);

    static const char* src = R"QML(
import Score 1.0
Script {
  ValueInlet { id: input; objectName: "In" }
  ValueOutlet { id: output; objectName: "Out" }
  tick: function(token, state) {
    var vs = input.values;
    for(var i = 0; i < vs.length; i++)
      output.addValue(vs[i].timestamp, vs[i].value);
  }
}
)QML";

    ossia::execution_state st;
    st.sampleRate = 48000;
    st.bufferSize = 64;

    auto node = std::make_shared<JS::js_node>(st);
    node->root_inputs().push_back(new ossia::value_inlet);
    node->root_outputs().push_back(new ossia::value_outlet);

    auto& in_port = *node->root_inputs()[0]->target<ossia::value_port>();
    auto& out_port = *node->root_outputs()[0]->target<ossia::value_port>();
    in_port.is_event = true;

    node->setScript({}, QString::fromUtf8(src));

    // Three messages inside one buffer, the way a device or an upstream node
    // delivers them.
    in_port.write_value(ossia::value{0.25f}, 0);
    in_port.write_value(ossia::value{0.5f}, 8);
    in_port.write_value(ossia::value{0.75f}, 16);
    REQUIRE(in_port.get_data().size() == 3);

    const ossia::exec_state_facade facade{&st};
    node->run(make_token(0, st.sampleRate, st.bufferSize), facade);

    INFO("the script saw " << out_port.get_data().size() << " value(s)");
    CHECK(out_port.get_data().size() == 3);
    if(out_port.get_data().size() == 3)
    {
      CHECK(out_port.get_data()[0].timestamp == 0);
      CHECK(out_port.get_data()[1].timestamp == 8);
      CHECK(out_port.get_data()[2].timestamp == 16);
    }

    node->clear();
  });
}
