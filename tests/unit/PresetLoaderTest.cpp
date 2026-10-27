// The preset harnesses (MidiPresetHarness.hpp, MidiPresetRig.hpp) put a preset
// straight into an object's inputs. Here the same presets go through score:
// Process::Preset::fromJson, the load-preset command a library drop submits,
// the control inlets it fills, and the avnd binding's conversion of each inlet
// value into its field. Both routes have to give the object the same values,
// or the preset tests check something score never does.

#include "MidiPresetRig.hpp"

#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Preset.hpp>
#include <Process/ProcessList.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>

#include <QFile>

#include <Advanced/MidiScaler/MidiDelay.hpp>
#include <Avnd/Factories.hpp>
#include <Crousti/ProcessModel.hpp>
#include <Fx/Arpeggiator_v2.hpp>
#include <Fx/Chord.hpp>
#include <Fx/Looper/MidiLooper.hpp>
#include <Fx/MidiUtil.hpp>
#include <Fx/Looper/ValueLooper.hpp>
#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/time_controls.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace
{
constexpr double tempo = 120.;

template <typename T>
bool same(const T& a, const T& b)
{
  if constexpr(std::is_floating_point_v<T>)
  {
    // The host carries a value as a float; the harness keeps the double.
    return std::abs(double(a) - double(b))
           <= 1e-4 * std::max(1., std::abs(double(a)));
  }
  else if constexpr(std::is_same_v<T, std::string> || std::is_enum_v<T>
                    || std::is_integral_v<T>)
  {
    return a == b;
  }
  else if constexpr(requires { a.has_value(); *a; })
  {
    return a.has_value() == b.has_value() && (!a.has_value() || same(*a, *b));
  }
  else if constexpr(requires { a.size(); a.begin(); a.end(); })
  {
    if(a.size() != b.size())
      return false;
    auto ib = b.begin();
    for(auto ia = a.begin(); ia != a.end(); ++ia, ++ib)
      if(!same(*ia, *ib))
        return false;
    return true;
  }
  else if constexpr(std::is_aggregate_v<T>)
  {
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
      return (same(avnd::pfr::get<I>(a), avnd::pfr::get<I>(b)) && ...);
    }(std::make_index_sequence<avnd::pfr::tuple_size_v<T>>{});
  }
  else
  {
    return a == b;
  }
}

//! What the execution hands field `field` for the inlet value `v`.
template <typename Field>
void convert(Field& field, const ossia::value& v)
{
  if constexpr(avnd::time_control_port<Field>)
  {
    // As oscr's node does for a time chooser: {time, mode}, mode 0 being
    // seconds and anything else a note value at the current tempo.
    ossia::vec2f vec;
    switch(v.get_type())
    {
      case ossia::val_type::FLOAT:
      case ossia::val_type::INT:
      case ossia::val_type::BOOL:
        vec = {ossia::convert<float>(v), 0.f};
        break;
      default:
        vec = ossia::convert<ossia::vec2f>(v);
        break;
    }
    const bool sync = vec[1] != 0.f;
    field.value = sync ? oscr::to_seconds(vec[0], tempo) : vec[0];
    if constexpr(requires { field.sync; })
      field.sync = sync;
  }
  else
  {
    oscr::from_ossia_value(field, v, field.value);
  }
}

template <typename Field>
bool same_field(const Field& a, const Field& b)
{
  if constexpr(requires { a.sync; })
    if(a.sync != b.sync)
      return false;
  return same(a.value, b.value);
}

//! A field a preset gives a value to: raw ports hold a pointer, not a value.
template <typename F>
concept preset_settable
    = requires(F& f) { f.value; }
      && !std::is_pointer_v<std::remove_cvref_t<decltype(std::declval<F&>().value)>>;

template <typename Info>
void check_folder(
    score::Document& doc, const QString& folder, const QString& uuid, bool as_host)
{
  auto& app = doc.context().app;
  auto& factories = app.interfaces<Process::ProcessFactoryList>();
  auto& load_preset = app.interfaces<Process::LoadPresetCommandFactoryList>();

  const auto presets = midi_presets::load(folder, uuid);
  REQUIRE(!presets.empty());

  auto* base = score::test::add_process(doc, uuid, {});
  REQUIRE(base);
  auto* proc = dynamic_cast<oscr::ProcessModel<Info>*>(base);
  REQUIRE(proc);

  for(const auto& p : presets)
  {
    INFO(folder.toStdString() << ": " << p.name.toStdString());

    QFile f{p.file};
    REQUIRE(f.open(QIODevice::ReadOnly));
    auto preset = Process::Preset::fromJson(factories, f.readAll());
    REQUIRE(preset);
    REQUIRE(proc->presetMatches(*preset));

    auto* cmd = load_preset.make(
        &Process::LoadPresetCommandFactory::make, *proc, *preset, doc.context());
    REQUIRE(cmd);
    CommandDispatcher<>{doc.context().commandStack}.submit(cmd);

    // Heap: some objects keep their buffers inline.
    auto expected_obj = std::make_unique<Info>();
    auto loaded_obj = std::make_unique<Info>();
    auto& expected = *expected_obj;
    auto& loaded = *loaded_obj;
    const auto errors = as_host ? midi_presets::apply_as_host(expected.inputs, p, tempo)
                                : midi_presets::apply(expected.inputs, p, tempo);
    CHECK(errors.join("; ").toStdString() == "");

    for(const auto& entry : p.values)
    {
      const int idx = entry.toArray()[0].toInt();
      INFO("entry " << idx);

      // The preset names inlets by id, the harness names fields by position.
      auto it = std::find_if(
          proc->inlets().begin(), proc->inlets().end(),
          [idx](auto* inl) { return inl->id().val() == idx; });
      REQUIRE(it != proc->inlets().end());
      auto* ctl = qobject_cast<Process::ControlInlet*>(*it);
      REQUIRE(ctl);
      const auto ports = proc->avnd_input_idx_to_model_ports(idx);
      REQUIRE(ports.size() == 1);
      CHECK(ports[0] == ctl);

      int n = 0;
      bool matched = false;
      avnd::pfr::for_each_field(loaded.inputs, [&](auto& field) {
        if(n++ != idx)
          return;
        // A port that is not a control, e.g. a raw value port, has no value
        // a preset sets.
        if constexpr(preset_settable<std::decay_t<decltype(field)>>)
        {
          convert(field, ctl->value());
          int k = 0;
          avnd::pfr::for_each_field(expected.inputs, [&](auto& exp) {
            if constexpr(std::is_same_v<
                             std::decay_t<decltype(exp)>, std::decay_t<decltype(field)>>)
              if(k == idx)
                matched = same_field(field, exp);
            k++;
          });
        }
      });
      CHECK(matched);
    }
  }
}
}

TEST_CASE("Presets load through score as the harnesses apply them", "[presets][loader]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    check_folder<Nodes::Arpeggiator::v2::Node>(
        *doc, "Arpeggiator", "4df48c1c-f5f2-4ab1-9f37-3b0adba139ae", true);
    check_folder<mtk::MidiDelay>(
        *doc, "Midi delay", "5ba4e20c-8936-43ab-aee5-8ac4555e05c1", true);
    check_folder<Nodes::Chord::Node>(
        *doc, "Chord", "F0904279-EA26-48DB-B0DF-F68FE3091DA1", false);
    check_folder<Nodes::MidiUtil::Node>(
        *doc, "Midi scale", "06b33b83-bb67-4f7a-9980-f5d66e4266c5", false);
    check_folder<Nodes::MidiLooper::Node>(
        *doc, "Looper (MIDI)", "0c0b7d3e-5a52-4b8f-9d1f-6e3a2b8c4f71", false);
    check_folder<Nodes::ValueLooper::Node>(
        *doc, "Looper (values)", "399796c5-9e86-4a00-8671-3e759ad6fb33", false);
  });
}
