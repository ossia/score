// Value-asserting DSP-correctness tests for score-plugin-faust.
//
// The plugin compiles Faust programs at runtime with libfaust's LLVM JIT
// (createDSPFactoryFromString, see Faust/EffectModel.cpp:296-337) using the
// options {"-double", "-vec"} (FAUSTFLOAT=double is set PUBLIC on the plugin
// target), then maps the dsp's UI onto score control ports through
// Faust::UI / Faust::UpdateUI (Faust/Utils.hpp).
//
// These tests exercise:
//  - the JIT compile+run path with the plugin's exact compiler options,
//    asserting sample-exact output for identity / gain / recursive filters
//  - control (slider) wiring: the zone value scales the output as expected
//  - Faust::UI and Faust::UpdateUI building/updating Process:: inlets/outlets
//  - stdfaust.lib import (oscillator frequency, lowpass transfer function)
//  - graceful failure on malformed programs, NaN/denormal robustness
//
// FaustEffectModel serialization round-trip needs a full application context
// (reload() reads library paths from score::AppContext()) — deferred to
// engine-level tests.

#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/detail/disable_fpe.hpp>

#include <QObject>

#include <Faust/Utils.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <faust/dsp/llvm-dsp.h>

#include <cmath>
#include <limits>
#include <map>
#include <random>
#include <string>
#include <vector>

using Catch::Approx;

namespace
{
// Compile + instantiate a Faust program the way the plugin does
// (EffectModel.cpp: "-double", "-vec", default target triple on x86_64).
struct Jit
{
  llvm_dsp_factory* fac{};
  llvm_dsp* dsp{};
  std::string err = std::string(4097, '\0');

  explicit Jit(const std::string& code, int rate = 48000)
  {
    // https://github.com/grame-cncm/faust/issues/1117 (mirrors the plugin)
    ossia::reset_default_fpu_state();

    const char* args[] = {"-double", "-vec"};
    fac = createDSPFactoryFromString("score_test", code, 2, args, "", err, -1);
    if(fac)
    {
      dsp = fac->createDSPInstance();
      if(dsp)
        dsp->init(rate);
    }
  }

  Jit(const Jit&) = delete;
  Jit& operator=(const Jit&) = delete;

  ~Jit()
  {
    delete dsp;
    if(fac)
      deleteDSPFactory(fac);
  }

  bool ok() const { return dsp != nullptr; }
  bool errored() const { return err[0] != '\0'; }

  // mono in -> mono out
  std::vector<double> run1(const std::vector<double>& in)
  {
    std::vector<double> out(in.size(), 0.0);
    double* ins[1] = {const_cast<double*>(in.data())};
    double* outs[1] = {out.data()};
    dsp->compute((int)in.size(), ins, outs);
    return out;
  }

  // generator (0 in) -> mono out
  std::vector<double> gen1(int frames)
  {
    std::vector<double> out(frames, 0.0);
    double* outs[1] = {out.data()};
    dsp->compute(frames, nullptr, outs);
    return out;
  }
};

// Captures the control zones of a jitted dsp by label.
struct ZoneUI final : ::UI
{
  std::map<std::string, FAUSTFLOAT*> zones;

  void openTabBox(const char*) override { }
  void openHorizontalBox(const char*) override { }
  void openVerticalBox(const char*) override { }
  void closeBox() override { }
  void declare(FAUSTFLOAT*, const char*, const char*) override { }
  void addSoundfile(const char*, const char*, Soundfile**) override { }
  void addButton(const char* l, FAUSTFLOAT* z) override { zones[l] = z; }
  void addCheckButton(const char* l, FAUSTFLOAT* z) override { zones[l] = z; }
  void addVerticalSlider(
      const char* l, FAUSTFLOAT* z, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT,
      FAUSTFLOAT) override
  {
    zones[l] = z;
  }
  void addHorizontalSlider(
      const char* l, FAUSTFLOAT* z, FAUSTFLOAT i, FAUSTFLOAT mn, FAUSTFLOAT mx,
      FAUSTFLOAT st) override
  {
    addVerticalSlider(l, z, i, mn, mx, st);
  }
  void addNumEntry(
      const char* l, FAUSTFLOAT* z, FAUSTFLOAT i, FAUSTFLOAT mn, FAUSTFLOAT mx,
      FAUSTFLOAT st) override
  {
    addVerticalSlider(l, z, i, mn, mx, st);
  }
  void addHorizontalBargraph(
      const char* l, FAUSTFLOAT* z, FAUSTFLOAT, FAUSTFLOAT) override
  {
    zones[l] = z;
  }
  void addVerticalBargraph(const char* l, FAUSTFLOAT* z, FAUSTFLOAT, FAUSTFLOAT) override
  {
    zones[l] = z;
  }
};

// Minimal stand-in for FaustEffectModel: just enough surface for
// Faust::UI / Faust::UpdateUI to build score control ports onto.
class FakeProc : public QObject
{
public:
  Process::Inlets m_inlets;
  Process::Outlets m_outlets;

  Process::Inlets& inlets() { return m_inlets; }
  Process::Outlets& outlets() { return m_outlets; }
  void controlAdded(const Id<Process::Port>&) { }
};

std::vector<double> ramp(int n)
{
  std::vector<double> v(n);
  for(int i = 0; i < n; i++)
    v[i] = 0.001 * i - 0.25;
  return v;
}
}

TEST_CASE("Faust JIT: identity and constant gain are sample-exact", "[faust][jit]")
{
  const auto in = ramp(512);

  SECTION("process = _;")
  {
    Jit fx{"process = _;"};
    REQUIRE(fx.ok());
    CHECK(fx.dsp->getNumInputs() == 1);
    CHECK(fx.dsp->getNumOutputs() == 1);

    const auto out = fx.run1(in);
    for(int i = 0; i < 512; i++)
      REQUIRE(out[i] == Approx(in[i]).margin(1e-15));
  }

  SECTION("process = _ * 0.5;")
  {
    Jit fx{"process = _ * 0.5;"};
    REQUIRE(fx.ok());

    const auto out = fx.run1(in);
    for(int i = 0; i < 512; i++)
      REQUIRE(out[i] == Approx(0.5 * in[i]).margin(1e-15));
  }
}

TEST_CASE(
    "Faust JIT: hslider default and updated values scale the signal",
    "[faust][jit][controls]")
{
  Jit fx{"process = _ * hslider(\"level\", 0.5, 0, 1, 0.01);"};
  REQUIRE(fx.ok());

  ZoneUI ui;
  fx.dsp->buildUserInterface(&ui);
  REQUIRE(ui.zones.count("level") == 1);

  // init() applied the declared default
  auto* zone = ui.zones["level"];
  CHECK(*zone == Approx(0.5).margin(1e-12));

  const auto in = ramp(256);
  auto out = fx.run1(in);
  for(int i = 0; i < 256; i++)
    REQUIRE(out[i] == Approx(0.5 * in[i]).margin(1e-15));

  // moving the control immediately changes the gain
  *zone = 0.25;
  out = fx.run1(in);
  for(int i = 0; i < 256; i++)
    REQUIRE(out[i] == Approx(0.25 * in[i]).margin(1e-15));
}

TEST_CASE(
    "Faust JIT: recursive one-pole matches the reference recurrence",
    "[faust][jit][filter]")
{
  // y[n] = x[n] + 0.5 * y[n-1]
  Jit fx{"process = + ~ *(0.5);"};
  REQUIRE(fx.ok());

  std::mt19937 rng{1234};
  std::uniform_real_distribution<double> dist{-1.0, 1.0};
  std::vector<double> in(512);
  for(auto& s : in)
    s = dist(rng);

  const auto out = fx.run1(in);

  double y = 0;
  for(int i = 0; i < 512; i++)
  {
    y = in[i] + 0.5 * y;
    REQUIRE(out[i] == Approx(y).margin(1e-12));
  }
}

TEST_CASE("Faust JIT: multi-channel routing is independent", "[faust][jit]")
{
  Jit fx{"process = *(2), *(3);"};
  REQUIRE(fx.ok());
  REQUIRE(fx.dsp->getNumInputs() == 2);
  REQUIRE(fx.dsp->getNumOutputs() == 2);

  const auto in0 = ramp(256);
  auto in1 = in0;
  for(auto& s : in1)
    s = -s + 0.125;

  std::vector<double> out0(256), out1(256);
  double* ins[2] = {const_cast<double*>(in0.data()), in1.data()};
  double* outs[2] = {out0.data(), out1.data()};
  fx.dsp->compute(256, ins, outs);

  for(int i = 0; i < 256; i++)
  {
    REQUIRE(out0[i] == Approx(2.0 * in0[i]).margin(1e-15));
    REQUIRE(out1[i] == Approx(3.0 * in1[i]).margin(1e-15));
  }
}

TEST_CASE(
    "Faust::UI maps the dsp controls onto score inlets/outlets",
    "[faust][plugin][controls]")
{
  Jit fx{"process = _ * hslider(\"level\", 0.5, 0, 1, 0.01) "
         "* checkbox(\"on\") * button(\"bang\") : hbargraph(\"outv\", 0, 1);"};
  REQUIRE(fx.ok());

  FakeProc proc;
  Faust::UI<FakeProc, false> ui{proc};
  fx.dsp->buildUserInterface(&ui);

  // UI callbacks arrive in program order: bang (button), on (checkbox),
  // level (slider) depend on faust's internal ordering; look them up by name.
  REQUIRE(proc.m_inlets.size() == 3);

  Process::FloatSlider* slider{};
  Process::Toggle* toggle{};
  Process::Button* button{};
  for(auto* inl : proc.m_inlets)
  {
    if(auto* s = dynamic_cast<Process::FloatSlider*>(inl))
      slider = s;
    else if(auto* t = dynamic_cast<Process::Toggle*>(inl))
      toggle = t;
    else if(auto* b = dynamic_cast<Process::Button*>(inl))
      button = b;
  }

  REQUIRE(slider);
  CHECK(slider->name() == "level");
  CHECK(slider->getMin() == Approx(0.f));
  CHECK(slider->getMax() == Approx(1.f));
  CHECK(ossia::convert<float>(slider->value()) == Approx(0.5f));

  REQUIRE(toggle);
  CHECK(toggle->name() == "on");

  REQUIRE(button);
  CHECK(button->name() == "bang");

  // The bargraph became a value outlet
  REQUIRE(proc.m_outlets.size() == 1);
  auto* bar = dynamic_cast<Process::Bargraph*>(proc.m_outlets[0]);
  REQUIRE(bar);
  CHECK(bar->name() == "outv");
}

TEST_CASE(
    "Faust::UpdateUI reuses, renames and replaces existing inlets",
    "[faust][plugin][controls]")
{
  FakeProc proc;
  // Slot 0 stands in for the main audio inlet of the real FaustEffectModel
  // (UpdateUI starts scanning at i == 1).
  proc.m_inlets.push_back(new Process::FloatSlider{
      0.f, 1.f, 0.f, "audio-placeholder", getStrongId(proc.m_inlets), &proc});

  // 1) initial build: one slider
  Jit fx1{"process = _ * hslider(\"level\", 0.5, 0, 1, 0.01);"};
  REQUIRE(fx1.ok());
  {
    Faust::UI<FakeProc, false> ui{proc};
    fx1.dsp->buildUserInterface(&ui);
    REQUIRE(proc.m_inlets.size() == 2);
  }
  auto* first = dynamic_cast<Process::FloatSlider*>(proc.m_inlets[1]);
  REQUIRE(first);

  // 2) same control type, new name/range/init: the inlet object is reused
  Jit fx2{"process = _ * hslider(\"volume\", 0.75, 0, 2, 0.01);"};
  REQUIRE(fx2.ok());
  {
    Process::Inlets toRemove;
    Process::Outlets toRemoveO;
    Faust::UpdateUI<FakeProc, true, false> ui{proc, toRemove, toRemoveO};
    fx2.dsp->buildUserInterface(&ui);

    CHECK(toRemove.empty());
    REQUIRE(proc.m_inlets.size() == 2);
    auto* updated = dynamic_cast<Process::FloatSlider*>(proc.m_inlets[1]);
    REQUIRE(updated == first); // reused in place
    CHECK(updated->name() == "volume");
    CHECK(updated->getMin() == Approx(0.f));
    CHECK(updated->getMax() == Approx(2.f));
    CHECK(ossia::convert<float>(updated->value()) == Approx(0.75f));
  }

  // 3) control type changes (slider -> button): the inlet is replaced and the
  // old one is queued for removal
  Jit fx3{"process = _ * button(\"mute\");"};
  REQUIRE(fx3.ok());
  {
    Process::Inlets toRemove;
    Process::Outlets toRemoveO;
    Faust::UpdateUI<FakeProc, true, false> ui{proc, toRemove, toRemoveO};
    fx3.dsp->buildUserInterface(&ui);

    REQUIRE(proc.m_inlets.size() == 2);
    CHECK(toRemove.size() == 1);
    CHECK(toRemove[0] == first);
    auto* btn = dynamic_cast<Process::Button*>(proc.m_inlets[1]);
    REQUIRE(btn);
    CHECK(btn->name() == "mute");
  }
}

TEST_CASE(
    "Faust JIT: stdfaust.lib import (oscillator frequency, lowpass transfer "
    "function)",
    "[faust][jit][stdlib]")
{
  constexpr int FS = 48000;

  Jit osc{"import(\"stdfaust.lib\"); process = os.osc(440.);", FS};
  if(!osc.ok())
  {
    WARN("libfaust could not resolve stdfaust.lib on this system, skipping: "
         + osc.err);
    return;
  }

  SECTION("os.osc(440) produces a 440 Hz unit sine")
  {
    REQUIRE(osc.dsp->getNumInputs() == 0);
    REQUIRE(osc.dsp->getNumOutputs() == 1);

    const auto out = osc.gen1(FS / 10); // 100 ms
    // 44 periods -> 88 sign changes
    int crossings = 0;
    for(int i = 1; i < (int)out.size(); i++)
      crossings += (out[i] > 0) != (out[i - 1] > 0);
    CHECK(crossings >= 86);
    CHECK(crossings <= 90);

    const double peak
        = std::abs(*std::max_element(out.begin(), out.end(), [](double a, double b) {
            return std::abs(a) < std::abs(b);
          }));
    CHECK(peak == Approx(1.0).margin(0.01));
  }

  SECTION("fi.lowpass(1, 1000): DC gain 1, -3 dB at cutoff, null at Nyquist")
  {
    Jit lp{"import(\"stdfaust.lib\"); process = fi.lowpass(1, 1000);", FS};
    REQUIRE(lp.ok());

    // DC: settles to 1
    const auto dc = lp.run1(std::vector<double>(4800, 1.0));
    CHECK(dc.back() == Approx(1.0).margin(1e-3));

    // 1 kHz (the cutoff): output RMS = input RMS / sqrt(2)
    Jit lp2{"import(\"stdfaust.lib\"); process = fi.lowpass(1, 1000);", FS};
    REQUIRE(lp2.ok());
    std::vector<double> tone(4800);
    for(int i = 0; i < 4800; i++)
      tone[i] = std::sin(2.0 * M_PI * 1000.0 * i / FS);
    const auto lp_out = lp2.run1(tone);
    double acc = 0;
    for(int i = 2400; i < 4800; i++) // steady state
      acc += lp_out[i] * lp_out[i];
    const double out_rms = std::sqrt(acc / 2400.0);
    CHECK(out_rms == Approx(0.5).margin(0.02)); // (1/sqrt2) * (1/sqrt2)

    // Nyquist: bilinear-transform lowpass has a zero at fs/2
    Jit lp3{"import(\"stdfaust.lib\"); process = fi.lowpass(1, 1000);", FS};
    REQUIRE(lp3.ok());
    std::vector<double> nyq(4800);
    for(int i = 0; i < 4800; i++)
      nyq[i] = (i % 2) ? -1.0 : 1.0;
    const auto ny_out = lp3.run1(nyq);
    double ny_peak = 0;
    for(int i = 4700; i < 4800; i++)
      ny_peak = std::max(ny_peak, std::abs(ny_out[i]));
    CHECK(ny_peak < 0.05);
  }
}

TEST_CASE(
    "Faust JIT: malformed programs fail gracefully with an error message",
    "[faust][jit][errors]")
{
  SECTION("syntax error")
  {
    Jit fx{"process = foo(;"};
    CHECK(fx.fac == nullptr);
    CHECK(fx.dsp == nullptr);
    CHECK(fx.errored());
  }

  SECTION("empty program")
  {
    Jit fx{""};
    CHECK(fx.dsp == nullptr);
  }

  SECTION("unknown identifier")
  {
    Jit fx{"process = definitely_not_a_faust_function;"};
    CHECK(fx.dsp == nullptr);
    CHECK(fx.errored());
  }
}

TEST_CASE(
    "Faust JIT: NaN and denormal input do not crash the jitted code",
    "[faust][jit][fuzz]")
{
  Jit fx{"process = _ * 0.5;"};
  REQUIRE(fx.ok());

  std::vector<double> in(256, std::numeric_limits<double>::quiet_NaN());
  auto out = fx.run1(in);
  CHECK(std::isnan(out[0]));

  std::fill(in.begin(), in.end(), 1e-320);
  out = fx.run1(in);
  for(auto v : out)
    CHECK(std::abs(v) <= 1e-320);

  std::fill(in.begin(), in.end(), std::numeric_limits<double>::infinity());
  out = fx.run1(in);
  CHECK(std::isinf(out[0]));

  SUCCEED("no crash on hostile input");
}

TEST_CASE(
    "Faust JIT: repeated compile/destroy cycles are stable",
    "[faust][jit][lifecycle]")
{
  for(int i = 0; i < 3; i++)
  {
    Jit fx{"process = _ * " + std::to_string(i + 1) + ";"};
    REQUIRE(fx.ok());
    const auto out = fx.run1({1.0, 2.0, 3.0});
    CHECK(out[2] == Approx(3.0 * (i + 1)).margin(1e-15));
  }
}
