// Every ExprTK preset shipped in the score user library, replayed through the
// real node it belongs to.
//
// This is a conformance sweep rather than a behaviour test: for each preset it
// asserts that the expression compiles (the node produces output at all), that
// the output has the shape the node promises, and — the part that matters — that
// no value is NaN or infinite. Two shipped presets used to fail exactly that
// last check: "Hypo 1" divided by a zero radius on its first element, and
// "Aggressive shaping" raised a bipolar audio sample to a fractional power.
//
// The corpus in ExpressionPresetCorpus.hpp is generated from the .scp files;
// see tools/gen-expression-preset-corpus.js.
#include "ExpressionPresetCorpus.hpp"

#include <Fx/Arraygen.hpp>
#include <Fx/Arraymap.hpp>
#include <Fx/MathAudioFilter.hpp>
#include <Fx/MathAudioGenerator.hpp>
#include <Fx/MathGenerator.hpp>
#include <Fx/MathValueFilter.hpp>
#include <Fx/MicroMapping.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using Catch::Approx;

namespace
{
// ---------------------------------------------------------------------------
// Harness
// ---------------------------------------------------------------------------

struct value_sink
{
  std::vector<ossia::value> values;

  template <typename Node>
  void wire(Node& n)
  {
    n.outputs.port.call.context = this;
    n.outputs.port.call.function = +[](void* ctx, ossia::value v) {
      static_cast<value_sink*>(ctx)->values.push_back(std::move(v));
    };
  }
};

halp::tick_flicks make_tick(int64_t start, int64_t end, double relpos)
{
  halp::tick_flicks tk{};
  tk.frames = 64;
  tk.start_in_flicks = start;
  tk.end_in_flicks = end;
  tk.relative_position = relpos;
  tk.parent_duration = 705600000;
  return tk;
}

//! Recursively checks that every number reachable from a node's output is a
//! real number, and reports where it isn't.
void check_finite(const ossia::value& v, const std::string& where)
{
  const auto num = [&](float f, int idx) {
    INFO(where << " [" << idx << "] = " << f);
    CHECK(std::isfinite(f));
  };

  switch(v.get_type())
  {
    case ossia::val_type::FLOAT:
      num(*v.target<float>(), 0);
      break;
    case ossia::val_type::INT:
    case ossia::val_type::BOOL:
    case ossia::val_type::IMPULSE:
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
        check_finite(l[i], where + "[" + std::to_string(i) + "]");
      break;
    }
    default: {
      INFO(where << ": unexpected output type " << (int)v.get_type());
      FAIL_CHECK("unexpected output type");
      break;
    }
  }
}

//! Flattens whatever a node emitted into plain numbers, so a preset that
//! silently degenerates to a constant can be spotted.
void flatten(const ossia::value& v, std::vector<float>& out)
{
  switch(v.get_type())
  {
    case ossia::val_type::FLOAT:
      out.push_back(*v.target<float>());
      break;
    case ossia::val_type::VEC2F:
      for(float f : *v.target<ossia::vec2f>())
        out.push_back(f);
      break;
    case ossia::val_type::VEC3F:
      for(float f : *v.target<ossia::vec3f>())
        out.push_back(f);
      break;
    case ossia::val_type::VEC4F:
      for(float f : *v.target<ossia::vec4f>())
        out.push_back(f);
      break;
    case ossia::val_type::LIST:
      for(auto& e : *v.target<std::vector<ossia::value>>())
        flatten(e, out);
      break;
    default:
      break;
  }
}

//! A generator that always emits the same number is not generating anything.
void check_not_constant(const std::vector<ossia::value>& values, const std::string& name)
{
  std::vector<float> flat;
  for(const auto& v : values)
    flatten(v, flat);

  INFO(name << ": " << flat.size() << " values, all equal to "
             << (flat.empty() ? 0.f : flat.front()));
  REQUIRE_FALSE(flat.empty());
  CHECK(std::any_of(
      flat.begin(), flat.end(), [&](float f) { return f != flat.front(); }));
}

//! A short animation: 24 ticks spanning one second of parent time.
template <typename F>
void for_each_tick(F&& f)
{
  constexpr int N = 24;
  for(int k = 0; k < N; k++)
  {
    const int64_t start = int64_t(705600000) * k / N;
    const int64_t end = int64_t(705600000) * (k + 1) / N;
    f(make_tick(start, end, double(k) / (N - 1)), k);
  }
}

//! Inputs a value-domain preset should survive: scalars of every flavour, then
//! vectors and lists of a few different lengths.
const std::vector<ossia::value>& value_inputs()
{
  static const std::vector<ossia::value> v{
      0.f,
      1.f,
      -1.f,
      0.5f,
      127.f,
      69.f,
      0.f,
      ossia::vec2f{0.25f, -0.75f},
      ossia::vec3f{1.f, 2.f, 3.f},
      ossia::vec4f{0.f, 0.25f, 0.5f, 1.f},
      ossia::value{std::vector<ossia::value>{0.1f, 0.2f, 0.3f, 0.4f}},
      ossia::value{std::vector<ossia::value>{-1.f, 1.f}},
      ossia::value{std::vector<ossia::value>{0.5f}},
      ossia::value{
          std::vector<ossia::value>{1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f}},
      0.5f,
  };
  return v;
}
}

// ===========================================================================
// Arraygen
// ===========================================================================

TEST_CASE("Presets: Arraygen", "[fx][exprtk][presets][arraygen]")
{
  for(const auto& p : preset_corpus::arraygen)
  {
    INFO("preset: " << p.name << "\n" << p.expr);

    Nodes::ArrayGenerator::Node node;
    value_sink out;
    out.wire(node);
    node.inputs.expr.value = p.expr;
    node.inputs.sz.value = p.size;

    for_each_tick([&](halp::tick_flicks tk, int) { node(tk); });

    // It compiled and ran.
    REQUIRE_FALSE(out.values.empty());

    for(std::size_t k = 0; k < out.values.size(); k++)
    {
      const auto& v = out.values[k];
      INFO("tick " << k);
      REQUIRE(v.get_type() == ossia::val_type::LIST);
      CHECK((int)v.target<std::vector<ossia::value>>()->size() == p.size);
      check_finite(v, p.name);
    }

    check_not_constant(out.values, p.name);
  }
}

// ===========================================================================
// Arraymap
// ===========================================================================

TEST_CASE("Presets: Arraymap", "[fx][exprtk][presets][arraymap]")
{
  for(const auto& p : preset_corpus::arraymap)
  {
    INFO("preset: " << p.name << "\n" << p.expr);

    Nodes::ArrayMapping::Node node;
    value_sink out;
    out.wire(node);
    node.inputs.expr.value = p.expr;

    int expected = 0;
    for_each_tick([&](halp::tick_flicks tk, int k) {
      const auto& in = value_inputs()[k % value_inputs().size()];
      node.inputs.port.value = in;
      node.trigger = true;
      node(tk);

      switch(in.get_type())
      {
        case ossia::val_type::VEC2F:
          expected = 2;
          break;
        case ossia::val_type::VEC3F:
          expected = 3;
          break;
        case ossia::val_type::VEC4F:
          expected = 4;
          break;
        case ossia::val_type::LIST:
          expected = (int)in.target<std::vector<ossia::value>>()->size();
          break;
        default:
          expected = 1;
          break;
      }

      REQUIRE_FALSE(out.values.empty());
      const auto& v = out.values.back();
      REQUIRE(v.get_type() == ossia::val_type::LIST);
      CHECK((int)v.target<std::vector<ossia::value>>()->size() == expected);
      check_finite(v, p.name);
    });
  }
}

// ===========================================================================
// Micromap
// ===========================================================================

TEST_CASE("Presets: Micromap", "[fx][exprtk][presets][micromap]")
{
  for(const auto& p : preset_corpus::micromap)
  {
    INFO("preset: " << p.name << "\n" << p.expr);

    Nodes::MicroMapping::Node node;
    value_sink out;
    out.wire(node);
    node.prepare(halp::setup{
        .input_channels = 0, .output_channels = 0, .frames = 64, .rate = 44100.});
    node.inputs.expr.value = p.expr;

    for_each_tick([&](halp::tick_flicks tk, int k) {
      node.inputs.port.value = value_inputs()[k % value_inputs().size()];
      node.trigger = true;
      node(tk);
    });

    // An expression indexing xv[2] cannot run against a one-element input, but
    // every preset must work for at least some of the inputs above.
    REQUIRE_FALSE(out.values.empty());
    for(const auto& v : out.values)
      check_finite(v, p.name);
  }
}

// ===========================================================================
// Expression Value Filter
// ===========================================================================

TEST_CASE("Presets: Expression Value Filter", "[fx][exprtk][presets][valuefilter]")
{
  for(const auto& p : preset_corpus::value_filter)
  {
    INFO("preset: " << p.name << "\n" << p.expr);

    Nodes::MathMapping::Node node;
    value_sink out;
    out.wire(node);
    node.prepare(halp::setup{
        .input_channels = 0, .output_channels = 0, .frames = 64, .rate = 44100.});
    node.inputs.expr.value = p.expr;
    node.inputs.a.value = p.a;
    node.inputs.b.value = p.b;
    node.inputs.c.value = p.c;

    for_each_tick([&](halp::tick_flicks tk, int k) {
      node.inputs.port.value = value_inputs()[k % value_inputs().size()];
      node.trigger = true;
      node(tk);
    });

    REQUIRE_FALSE(out.values.empty());
    for(const auto& v : out.values)
      check_finite(v, p.name);
  }
}

TEST_CASE("Presets: Expression Value Filter, swept parameters", "[fx][exprtk][presets][valuefilter]")
{
  // The stored a/b/c are one point in the space; the sliders can go anywhere.
  for(const auto& p : preset_corpus::value_filter)
  {
    INFO("preset: " << p.name << "\n" << p.expr);

    for(float v : {0.f, 0.5f, 1.f})
    {
      INFO("a = b = c = " << v);
      Nodes::MathMapping::Node node;
      value_sink out;
      out.wire(node);
      node.prepare(halp::setup{
          .input_channels = 0, .output_channels = 0, .frames = 64, .rate = 44100.});
      node.inputs.expr.value = p.expr;
      node.inputs.a.value = v;
      node.inputs.b.value = v;
      node.inputs.c.value = v;

      for_each_tick([&](halp::tick_flicks tk, int k) {
        node.inputs.port.value = value_inputs()[k % value_inputs().size()];
        node.trigger = true;
        node(tk);
      });

      for(const auto& r : out.values)
        check_finite(r, p.name);
    }
  }
}

// ===========================================================================
// Expression Value Generator
// ===========================================================================

TEST_CASE("Presets: Expression Value Generator", "[fx][exprtk][presets][valuegen]")
{
  for(const auto& p : preset_corpus::value_generator)
  {
    INFO("preset: " << p.name << "\n" << p.expr);

    Nodes::MathGenerator::Node node;
    value_sink out;
    out.wire(node);
    node.inputs.expr.value = p.expr;
    node.inputs.a.value = p.a;
    node.inputs.b.value = p.b;
    node.inputs.c.value = p.c;

    for_each_tick([&](halp::tick_flicks tk, int) { node(tk); });

    REQUIRE_FALSE(out.values.empty());
    for(const auto& v : out.values)
      check_finite(v, p.name);

    // A generator must generate. Some presets ("XY", "XYZ") are driven purely
    // by their sliders and are constant while those are, so sweep them too.
    value_sink swept;
    swept.wire(node);
    swept.values = out.values;
    for_each_tick([&](halp::tick_flicks tk, int k) {
      node.inputs.a.value = k / 23.f;
      node.inputs.b.value = 1.f - k / 23.f;
      node.inputs.c.value = (k % 5) / 4.f;
      node(tk);
    });
    check_not_constant(swept.values, p.name);
  }
}

TEST_CASE("Presets: Expression Value Generator, long run", "[fx][exprtk][presets][valuegen]")
{
  // The stateful ones (attractors, drunk walks, sample & hold) only misbehave
  // after a while: run each preset for a few thousand ticks at the extremes of
  // its parameter range and check it never leaves the reals.
  for(const auto& p : preset_corpus::value_generator)
  {
    INFO("preset: " << p.name << "\n" << p.expr);

    for(float v : {0.f, 1.f})
    {
      INFO("a = b = c = " << v);
      Nodes::MathGenerator::Node node;
      value_sink out;
      out.wire(node);
      node.inputs.expr.value = p.expr;
      node.inputs.a.value = v;
      node.inputs.b.value = v;
      node.inputs.c.value = v;

      constexpr int N = 4000;
      for(int k = 0; k < N; k++)
      {
        const int64_t start = int64_t(705600000) * 10 * k / N;
        node(make_tick(start, start + 176400, double(k) / (N - 1)));
      }

      REQUIRE_FALSE(out.values.empty());
      check_finite(out.values.back(), p.name);
      check_finite(out.values[out.values.size() / 2], p.name);
    }
  }
}

// ===========================================================================
// Expression Audio Generator
// ===========================================================================

namespace
{
struct audio_buffers
{
  static constexpr int frames = 256;
  std::vector<std::vector<double>> data;
  std::vector<double*> ptrs;

  explicit audio_buffers(int channels)
      : data(channels, std::vector<double>(frames, 0.))
  {
    for(auto& c : data)
      ptrs.push_back(c.data());
  }

  void fill_sine()
  {
    for(std::size_t c = 0; c < data.size(); c++)
      for(int i = 0; i < frames; i++)
        data[c][i] = 0.8 * std::sin(2. * 3.14159265358979 * (110. + 40. * c) * i / 44100.);
  }

  void check(const std::string& where, double bound) const
  {
    for(std::size_t c = 0; c < data.size(); c++)
      for(int i = 0; i < frames; i++)
      {
        if(!std::isfinite(data[c][i]) || std::abs(data[c][i]) > bound)
        {
          INFO(where << " channel " << c << " frame " << i << " = " << data[c][i]);
          FAIL_CHECK("sample out of range");
          return;
        }
      }
  }
};
}

TEST_CASE("Presets: Expression Audio Generator", "[fx][exprtk][presets][audiogen]")
{
  for(const auto& p : preset_corpus::audio_generator)
  {
    INFO("preset: " << p.name << "\n" << p.expr);

    audio_buffers out{2};
    Nodes::MathAudioGenerator::Node node;
    node.outputs.audio.request_channels = [](int) {};
    node.prepare(halp::setup{
        .input_channels = 0,
        .output_channels = 2,
        .frames = audio_buffers::frames,
        .rate = 44100.});
    node.outputs.audio.samples = out.ptrs.data();
    node.outputs.audio.channels = 2;
    node.inputs.expr.value = p.expr;
    node.inputs.a.value = p.a;
    node.inputs.b.value = p.b;
    node.inputs.c.value = p.c;

    bool any = false;
    for(int block = 0; block < 40; block++)
    {
      halp::tick_flicks tk{};
      tk.frames = audio_buffers::frames;
      node(tk);
      // A generator must be audible, and must stay inside a sane range.
      out.check(p.name, 4.);
      for(auto& ch : out.data)
        for(double s : ch)
          if(s != 0.)
            any = true;
    }
    CHECK(any);
  }
}

// ===========================================================================
// Expression Audio Filter
// ===========================================================================

TEST_CASE("Presets: Expression Audio Filter", "[fx][exprtk][presets][audiofilter]")
{
  for(const auto& p : preset_corpus::audio_filter)
  {
    for(int channels : {1, 2, 6})
    {
      INFO("preset: " << p.name << " (" << channels << " ch)\n" << p.expr);

      audio_buffers in{channels}, out{channels};
      in.fill_sine();

      Nodes::MathAudioFilter::Node node;
      node.inputs.audio.samples = in.ptrs.data();
      node.inputs.audio.channels = channels;
      node.outputs.audio.samples = out.ptrs.data();
      node.outputs.audio.channels = channels;
      node.prepare(halp::setup{
          .input_channels = channels,
          .output_channels = channels,
          .frames = audio_buffers::frames,
          .rate = 44100.});
      node.inputs.expr.value = p.expr;
      node.inputs.a.value = p.a;
      node.inputs.b.value = p.b;
      node.inputs.c.value = p.c;

      bool audible = false;
      for(int block = 0; block < 8; block++)
      {
        halp::tick_flicks tk{};
        tk.frames = audio_buffers::frames;
        node(tk);
        out.check(p.name, 64.);
        for(auto& ch : out.data)
          for(double s : ch)
            if(s != 0.)
              audible = true;
      }
      // A filter fed a full-scale sine must write something.
      CHECK(audible);
    }
  }
}

TEST_CASE("Presets: Expression Audio Filter, extreme input", "[fx][exprtk][presets][audiofilter]")
{
  // Silence, full scale, and a hard step: the shapers must not blow up.
  for(const auto& p : preset_corpus::audio_filter)
  {
    INFO("preset: " << p.name << "\n" << p.expr);

    audio_buffers in{2}, out{2};
    for(std::size_t c = 0; c < in.data.size(); c++)
      for(int i = 0; i < audio_buffers::frames; i++)
        in.data[c][i] = (i < 64) ? 0. : ((i < 128) ? 1. : ((i < 192) ? -1. : 0.));

    Nodes::MathAudioFilter::Node node;
    node.inputs.audio.samples = in.ptrs.data();
    node.inputs.audio.channels = 2;
    node.outputs.audio.samples = out.ptrs.data();
    node.outputs.audio.channels = 2;
    node.prepare(halp::setup{
        .input_channels = 2,
        .output_channels = 2,
        .frames = audio_buffers::frames,
        .rate = 44100.});
    node.inputs.expr.value = p.expr;

    for(float v : {0.f, 0.5f, 1.f})
    {
      INFO("a = b = c = " << v);
      node.inputs.a.value = v;
      node.inputs.b.value = v;
      node.inputs.c.value = v;
      for(int block = 0; block < 4; block++)
      {
        halp::tick_flicks tk{};
        tk.frames = audio_buffers::frames;
        node(tk);
        out.check(p.name, 64.);
      }
    }
  }
}

// ===========================================================================
// Targeted checks: a preset that is finite and non-constant can still be wrong.
// ===========================================================================

namespace
{
template <std::size_t N>
const preset_corpus::entry&
find(const preset_corpus::entry (&arr)[N], std::string_view name)
{
  for(const auto& e : arr)
    if(name == e.name)
      return e;
  throw std::runtime_error("no such preset: " + std::string(name));
}

//! Runs one Arraygen preset for a single tick and returns what it produced.
std::vector<ossia::value> run_arraygen(std::string_view name, double relpos = 0.)
{
  const auto& p = find(preset_corpus::arraygen, name);
  Nodes::ArrayGenerator::Node node;
  value_sink out;
  out.wire(node);
  node.inputs.expr.value = p.expr;
  node.inputs.sz.value = p.size;
  node(make_tick(0, 29400000, relpos));

  REQUIRE(out.values.size() == 1);
  REQUIRE(out.values[0].get_type() == ossia::val_type::LIST);
  return *out.values[0].target<std::vector<ossia::value>>();
}

//! Pushes values through one Micromap preset and returns the outputs.
std::vector<float>
run_micromap(std::string_view name, const std::vector<ossia::value>& inputs)
{
  const auto& p = find(preset_corpus::micromap, name);
  Nodes::MicroMapping::Node node;
  value_sink out;
  out.wire(node);
  node.prepare(halp::setup{
      .input_channels = 0, .output_channels = 0, .frames = 64, .rate = 44100.});
  node.inputs.expr.value = p.expr;

  for(std::size_t k = 0; k < inputs.size(); k++)
  {
    node.inputs.port.value = inputs[k];
    node.trigger = true;
    node(make_tick(int64_t(k) * 705600000, int64_t(k + 1) * 705600000, 0.));
  }

  std::vector<float> res;
  for(const auto& v : out.values)
    res.push_back(*v.target<float>());
  return res;
}

//! Pushes values through one Expression Value Filter preset, at its own a/b/c.
std::vector<float>
run_value_filter(std::string_view name, const std::vector<float>& inputs)
{
  const auto& p = find(preset_corpus::value_filter, name);
  Nodes::MathMapping::Node node;
  value_sink out;
  out.wire(node);
  node.prepare(halp::setup{
      .input_channels = 0, .output_channels = 0, .frames = 64, .rate = 44100.});
  node.inputs.expr.value = p.expr;
  node.inputs.a.value = p.a;
  node.inputs.b.value = p.b;
  node.inputs.c.value = p.c;

  for(std::size_t k = 0; k < inputs.size(); k++)
  {
    node.inputs.port.value = inputs[k];
    node.trigger = true;
    node(make_tick(int64_t(k) * 14700000, int64_t(k + 1) * 14700000, 0.));
  }

  std::vector<float> res;
  for(const auto& v : out.values)
    res.push_back(*v.target<float>());
  return res;
}

float radius(const ossia::value& v)
{
  auto& p = *v.target<ossia::vec2f>();
  return std::hypot(p[0], p[1]);
}
}

TEST_CASE("Presets: Arraygen shapes are the shapes they claim", "[fx][exprtk][presets][arraygen]")
{
  SECTION("Circle: every point is on the unit circle")
  {
    const auto l = run_arraygen("Circle");
    REQUIRE(l.size() == 64);
    for(auto& v : l)
      CHECK(radius(v) == Approx(1.).epsilon(1e-5));
    CHECK((*l[0].target<ossia::vec2f>())[0] == Approx(1.f));
    CHECK((*l[16].target<ossia::vec2f>())[1] == Approx(1.f).margin(1e-5));
  }

  SECTION("Fibonacci sphere: every point is on the unit sphere")
  {
    const auto l = run_arraygen("Fibonacci sphere");
    REQUIRE(l.size() == 512);
    for(auto& v : l)
    {
      REQUIRE(v.get_type() == ossia::val_type::VEC3F);
      auto& p = *v.target<ossia::vec3f>();
      CHECK(
          std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2])
          == Approx(1.).epsilon(1e-4));
    }
  }

  SECTION("Fibonacci disc: inside the unit disc, and spread over all of it")
  {
    const auto l = run_arraygen("Fibonacci disc");
    REQUIRE(l.size() == 512);
    float rmin = 10.f, rmax = 0.f;
    for(auto& v : l)
    {
      const auto r = radius(v);
      CHECK(r <= 1.0001f);
      rmin = std::min(rmin, r);
      rmax = std::max(rmax, r);
    }
    CHECK(rmin < 0.05f);
    CHECK(rmax > 0.95f);
  }

  SECTION("Grid: a square lattice spanning the unit square")
  {
    const auto l = run_arraygen("Grid");
    REQUIRE(l.size() == 256); // 16 x 16
    CHECK((*l[0].target<ossia::vec2f>())[0] == Approx(-1.f));
    CHECK((*l[0].target<ossia::vec2f>())[1] == Approx(-1.f));
    CHECK((*l[255].target<ossia::vec2f>())[0] == Approx(1.f));
    CHECK((*l[255].target<ossia::vec2f>())[1] == Approx(1.f));
    // The first row shares its y.
    for(int i = 0; i < 16; i++)
      CHECK((*l[i].target<ossia::vec2f>())[1] == Approx(-1.f));
  }

  SECTION("Line: an evenly spaced segment from -1 to 1")
  {
    const auto l = run_arraygen("Line");
    REQUIRE(l.size() == 32);
    CHECK((*l[0].target<ossia::vec2f>())[0] == Approx(-1.f));
    CHECK((*l[31].target<ossia::vec2f>())[0] == Approx(1.f));
    const float step
        = (*l[1].target<ossia::vec2f>())[0] - (*l[0].target<ossia::vec2f>())[0];
    for(int i = 1; i < 32; i++)
      CHECK(
          (*l[i].target<ossia::vec2f>())[0] - (*l[i - 1].target<ossia::vec2f>())[0]
          == Approx(step));
  }

  SECTION("Rainbow: valid colours, and the hue really goes round")
  {
    const auto l = run_arraygen("Rainbow");
    REQUIRE(l.size() == 60);
    bool saturated = false, dark = false;
    for(auto& v : l)
    {
      REQUIRE(v.get_type() == ossia::val_type::VEC3F);
      auto& c = *v.target<ossia::vec3f>();
      for(int k = 0; k < 3; k++)
      {
        CHECK(c[k] >= 0.f);
        CHECK(c[k] <= 1.f);
      }
      if(c[0] > 0.99f)
        saturated = true;
      if(c[0] < 0.01f)
        dark = true;
    }
    CHECK(saturated);
    CHECK(dark);
  }

  SECTION("Chase: a single bright spot, at the start of the strip at t = 0")
  {
    const auto l = run_arraygen("Chase");
    int best = 0;
    float peak = -1.f;
    for(std::size_t i = 0; i < l.size(); i++)
    {
      const float f = *l[i].target<float>();
      CHECK(f >= 0.f);
      CHECK(f <= 1.0001f);
      if(f > peak)
      {
        peak = f;
        best = (int)i;
      }
    }
    CHECK(peak > 0.9f);
    CHECK(best == 0);
  }

  SECTION("Chromatic scale: neighbours are a semitone apart, centred on A440")
  {
    const auto l = run_arraygen("Chromatic scale");
    REQUIRE(l.size() == 24);
    for(std::size_t i = 1; i < l.size(); i++)
      CHECK(
          *l[i].target<float>() / *l[i - 1].target<float>()
          == Approx(std::pow(2.f, 1.f / 12.f)));
    CHECK(*l[12].target<float>() == Approx(440.f));
  }

  SECTION("Harmonic series: 1, 1/2, 1/3, ...")
  {
    const auto l = run_arraygen("Harmonic series");
    REQUIRE(l.size() == 16);
    for(std::size_t i = 0; i < l.size(); i++)
      CHECK(*l[i].target<float>() == Approx(1.f / (1 + i)));
  }
}

TEST_CASE("Presets: Micromap conversions are numerically right", "[fx][exprtk][presets][micromap]")
{
  const auto one = [](std::string_view name, float in) {
    const auto r = run_micromap(name, {in});
    REQUIRE(r.size() == 1);
    return r.back();
  };

  CHECK(one("Frequency to MIDI pitch", 440.f) == Approx(69.f));
  CHECK(one("Frequency to MIDI pitch", 880.f) == Approx(81.f));
  CHECK(one("Semitones to ratio", 12.f) == Approx(2.f));
  CHECK(one("Semitones to ratio", 0.f) == Approx(1.f));
  CHECK(one("BPM to seconds", 120.f) == Approx(0.5f));
  CHECK(one("BPM to Hz", 120.f) == Approx(2.f));
  CHECK(one("Milliseconds to Hz", 500.f) == Approx(2.f));
  CHECK(one("Square root", 16.f) == Approx(4.f));
  CHECK(one("Square root", -4.f) == Approx(0.f)); // and not NaN
  CHECK(one("Clamp 0-1", 5.f) == Approx(1.f));
  CHECK(one("Clamp 0-1", -5.f) == Approx(0.f));
  CHECK(one("Invert", 0.25f) == Approx(0.75f));
  CHECK(one("Bipolar to unipolar", -1.f) == Approx(0.f));
  CHECK(one("Unipolar to bipolar", 0.f) == Approx(-1.f));
  CHECK(one("Wrap 0-1", 3.25f) == Approx(0.25f));
  CHECK(one("Smoothstep", 0.5f) == Approx(0.5f));
  CHECK(one("Smoothstep", 2.f) == Approx(1.f));
  CHECK(one("Perceptual fader", 0.f) == Approx(0.f));
  CHECK(one("Perceptual fader", 1.f) == Approx(1.f));

  SECTION("Fold -1 to 1 reflects out-of-range values back in")
  {
    CHECK(one("Fold -1 to 1", 0.f) == Approx(0.f).margin(1e-6));
    CHECK(one("Fold -1 to 1", 1.f) == Approx(1.f));
    CHECK(one("Fold -1 to 1", 1.5f) == Approx(0.5f));
    CHECK(one("Fold -1 to 1", 3.f) == Approx(-1.f));
    for(float v = -8.f; v <= 8.f; v += 0.13f)
    {
      const auto r = one("Fold -1 to 1", v);
      CHECK(r >= -1.0001f);
      CHECK(r <= 1.0001f);
    }
  }

  SECTION("Array aggregates")
  {
    const ossia::value pair{std::vector<ossia::value>{3.f, 4.f}};
    const ossia::value spread{std::vector<ossia::value>{1.f, 2.f, 3.f, 4.f}};

    CHECK(run_micromap("Vector length", {pair}).back() == Approx(5.f));
    CHECK(run_micromap("Sum", {spread}).back() == Approx(10.f));
    CHECK(run_micromap("Average", {spread}).back() == Approx(2.5f));
    CHECK(run_micromap("Maximum", {spread}).back() == Approx(4.f));
    CHECK(run_micromap("Minimum", {spread}).back() == Approx(1.f));
    CHECK(run_micromap("RMS", {spread}).back() == Approx(std::sqrt(30.f / 4.f)));
  }

  SECTION("Stateful mappings")
  {
    // Smooth converges on a step without overshooting it.
    const auto sm = run_micromap("Smooth", std::vector<ossia::value>(200, 1.f));
    CHECK(sm.front() < 0.2f);
    CHECK(sm.back() == Approx(1.f).margin(1e-4));
    for(std::size_t i = 1; i < sm.size(); i++)
      CHECK(sm[i] >= sm[i - 1]);

    // Slew limit never moves by more than its step.
    const auto sl = run_micromap("Slew limit", std::vector<ossia::value>(10, 1.f));
    for(std::size_t i = 1; i < sl.size(); i++)
      CHECK(std::abs(sl[i] - sl[i - 1]) <= 0.0201f);

    // Direction reports the sign of the change.
    const auto dir = run_micromap("Direction", {0.f, 1.f, 1.f, 0.5f});
    REQUIRE(dir.size() == 4);
    CHECK(dir[1] == Approx(1.f));
    CHECK(dir[2] == Approx(0.f));
    CHECK(dir[3] == Approx(-1.f));

    // Peak hold jumps up instantly and decays afterwards.
    const auto pk = run_micromap("Peak hold", {0.f, 1.f, 0.f, 0.f, 0.f});
    REQUIRE(pk.size() == 5);
    CHECK(pk[1] == Approx(1.f));
    CHECK(pk[2] == Approx(0.99f));
    CHECK(pk[3] < pk[2]);
  }
}

TEST_CASE("Presets: Expression Value Filter behaviour", "[fx][exprtk][presets][valuefilter]")
{
  SECTION("Smoothstep ramps between a and b and stops there")
  {
    // Stored a = 0, b = 1.
    const auto r = run_value_filter("Smoothstep", {-5.f, 0.f, 0.5f, 1.f, 5.f});
    REQUIRE(r.size() == 5);
    CHECK(r[0] == Approx(0.f));
    CHECK(r[1] == Approx(0.f));
    CHECK(r[2] == Approx(0.5f));
    CHECK(r[3] == Approx(1.f));
    CHECK(r[4] == Approx(1.f)); // used to run away past the top end
  }

  SECTION("Schmitt trigger has hysteresis")
  {
    // Stored a = 0.4 (falling edge), b = 0.6 (rising edge).
    const auto r
        = run_value_filter("Schmitt trigger", {0.f, 0.5f, 0.7f, 0.5f, 0.3f, 0.5f});
    REQUIRE(r.size() == 6);
    CHECK(r[0] == Approx(0.f));
    CHECK(r[1] == Approx(0.f)); // inside the band: stays low
    CHECK(r[2] == Approx(1.f)); // above 0.6: latches high
    CHECK(r[3] == Approx(1.f)); // inside the band: stays high
    CHECK(r[4] == Approx(0.f)); // below 0.4: latches low
    CHECK(r[5] == Approx(0.f));
  }

  SECTION("Toggle on press flips once per rising edge")
  {
    const auto r
        = run_value_filter("Toggle on press", {0.f, 1.f, 1.f, 0.f, 1.f, 0.f});
    REQUIRE(r.size() == 6);
    CHECK(r[1] == Approx(1.f));
    CHECK(r[2] == Approx(1.f));
    CHECK(r[3] == Approx(1.f));
    CHECK(r[4] == Approx(0.f));
  }

  SECTION("One-pole smoothing converges monotonically")
  {
    const auto r
        = run_value_filter("One-pole smoothing", std::vector<float>(400, 1.f));
    CHECK(r.front() < 0.2f);
    CHECK(r.back() == Approx(1.f).margin(1e-3));
    for(std::size_t i = 1; i < r.size(); i++)
      CHECK(r[i] >= r[i - 1]);
  }

  SECTION("Quantize to major scale only emits scale degrees")
  {
    static const int major[]{0, 2, 4, 5, 7, 9, 11};
    for(float v = 0.f; v <= 1.f; v += 0.017f)
    {
      const auto r = run_value_filter("Quantize to major scale", {v});
      REQUIRE(r.size() == 1);
      const int note = (int)std::lround(r.back());
      INFO("input " << v << " -> note " << note);
      CHECK(note >= 36);
      CHECK(note <= 83);
      const int deg = ((note - 36) % 12 + 12) % 12;
      CHECK(std::find(std::begin(major), std::end(major), deg) != std::end(major));
    }
  }

  SECTION("Remap sends 0..1 onto a..b")
  {
    // Stored a = 0, b = 1: the identity.
    const auto r = run_value_filter("Remap to a-b", {0.f, 0.25f, 1.f});
    REQUIRE(r.size() == 3);
    CHECK(r[0] == Approx(0.f));
    CHECK(r[1] == Approx(0.25f));
    CHECK(r[2] == Approx(1.f));
  }
}

TEST_CASE("Presets: Expression Audio Filter behaviour", "[fx][exprtk][presets][audiofilter]")
{
  const auto process
      = [](std::string_view name, std::vector<double> mono, float a, float b, float c) {
          const auto& p = find(preset_corpus::audio_filter, name);
          std::vector<double> outbuf(mono.size(), 0.);
          double* ins[1]{mono.data()};
          double* outs[1]{outbuf.data()};

          Nodes::MathAudioFilter::Node node;
          node.inputs.audio.samples = ins;
          node.inputs.audio.channels = 1;
          node.outputs.audio.samples = outs;
          node.outputs.audio.channels = 1;
          node.prepare(halp::setup{
              .input_channels = 1,
              .output_channels = 1,
              .frames = (int)mono.size(),
              .rate = 44100.});
          node.inputs.expr.value = p.expr;
          node.inputs.a.value = a;
          node.inputs.b.value = b;
          node.inputs.c.value = c;

          halp::tick_flicks tk{};
          tk.frames = (int)mono.size();
          node(tk);
          return outbuf;
        };

  SECTION("Invert phase is exact")
  {
    const auto r = process("Invert phase", {1., -0.5, 0.25}, 0.5f, 0.5f, 0.5f);
    CHECK(r[0] == -1.);
    CHECK(r[1] == 0.5);
    CHECK(r[2] == -0.25);
  }

  SECTION("Rectify")
  {
    const auto r = process("Rectify", {1., -0.5, 0.}, 0.5f, 0.5f, 0.5f);
    CHECK(r[0] == 1.);
    CHECK(r[1] == 0.5);
    CHECK(r[2] == 0.);
  }

  SECTION("Half rectify keeps the positive half only")
  {
    const auto r = process("Half rectify", {1., -0.5, 0.25}, 0.5f, 0.5f, 0.5f);
    CHECK(r[0] == 1.);
    CHECK(r[1] == 0.);
    CHECK(r[2] == 0.25);
  }

  SECTION("Gain is the advertised decibels")
  {
    // a = 0 is -60 dB, a = 5/6 is unity, a = 1 is +12 dB.
    CHECK(process("Gain", {1.}, 0.f, 0.5f, 0.5f)[0] == Approx(0.001).epsilon(1e-6));
    CHECK(
        process("Gain", {1.}, 5.f / 6.f, 0.5f, 0.5f)[0] == Approx(1.).epsilon(1e-5));
    CHECK(
        process("Gain", {1.}, 1.f, 0.5f, 0.5f)[0]
        == Approx(std::pow(10., 12. / 20.)).epsilon(1e-6));
  }

  SECTION("Soft clip stays inside -1..1 for any input")
  {
    for(float a : {0.f, 0.5f, 1.f})
    {
      const auto r = process("Soft clip", {0., 1., -1., 20., -20.}, a, 0.5f, 0.5f);
      for(double s : r)
      {
        CHECK(std::abs(s) <= 1.0001);
        CHECK(std::isfinite(s));
      }
      CHECK(r[0] == Approx(0.).margin(1e-12)); // silence in, silence out
    }
  }

  SECTION("The Chebyshev shapers stay bounded, whatever the drive")
  {
    for(const char* name : {"Cheby 2", "Cheby 3", "Cheby 4", "Shape B"})
    {
      INFO(name);
      for(float a : {0.f, 0.5f, 1.f})
      {
        const auto r = process(name, {0., 0.5, -0.5, 1., -1., 8.}, a, 1.f, 1.f);
        for(double s : r)
        {
          INFO("a = " << a << " sample " << s);
          CHECK(std::abs(s) <= 1.0001); // used to reach 8 x 10^8
        }
      }
    }
  }

  SECTION("Discretize keeps the signal, whatever a is")
  {
    for(float a : {0.f, 0.5f, 1.f})
    {
      const auto r = process("Discretize", {0., 0.5, -0.5, 1.}, a, 0.f, 0.f);
      for(double s : r)
        CHECK(std::isfinite(s)); // a == 0 used to give 0 / 0
      CHECK(std::abs(r[3]) == Approx(1.).epsilon(0.5));
    }
  }

  SECTION("Mono averages the channels")
  {
    const auto& p = find(preset_corpus::audio_filter, "Mono");
    std::vector<double> l{1., 1.}, r{-1., 3.}, ol(2, 0.), orr(2, 0.);
    double* ins[2]{l.data(), r.data()};
    double* outs[2]{ol.data(), orr.data()};

    Nodes::MathAudioFilter::Node node;
    node.inputs.audio.samples = ins;
    node.inputs.audio.channels = 2;
    node.outputs.audio.samples = outs;
    node.outputs.audio.channels = 2;
    node.prepare(halp::setup{
        .input_channels = 2, .output_channels = 2, .frames = 2, .rate = 44100.});
    node.inputs.expr.value = p.expr;

    halp::tick_flicks tk{};
    tk.frames = 2;
    node(tk);

    CHECK(ol[0] == Approx(0.));
    CHECK(orr[0] == Approx(0.));
    CHECK(ol[1] == Approx(2.));
    CHECK(orr[1] == Approx(2.));
  }

  SECTION("The disto presets work on a mono bus")
  {
    // They used to write out[1] unconditionally, which cannot compile against a
    // single channel: the node then produced nothing at all.
    for(const char* name :
        {"Crude Lowpass", "Cubic Disto", "Harsh", "Sin Disto", "Tan Disto"})
    {
      INFO(name);
      const auto r = process(name, {0.5, -0.5, 1., -1.}, 0.5f, 0.5f, 0.5f);
      bool any = false;
      for(double s : r)
      {
        CHECK(std::isfinite(s));
        CHECK(std::abs(s) <= 1.0001);
        if(s != 0.)
          any = true;
      }
      CHECK(any);
    }
  }
}

TEST_CASE("Presets: Expression Audio Generator behaviour", "[fx][exprtk][presets][audiogen]")
{
  const auto render = [](std::string_view name, int frames, float a, float b, float c) {
    const auto& p = find(preset_corpus::audio_generator, name);
    std::vector<double> l(frames, 0.), r(frames, 0.);
    double* outs[2]{l.data(), r.data()};

    Nodes::MathAudioGenerator::Node node;
    node.outputs.audio.request_channels = [](int) {};
    node.prepare(halp::setup{
        .input_channels = 0, .output_channels = 2, .frames = frames, .rate = 44100.});
    node.outputs.audio.samples = outs;
    node.outputs.audio.channels = 2;
    node.inputs.expr.value = p.expr;
    node.inputs.a.value = a;
    node.inputs.b.value = b;
    node.inputs.c.value = c;

    halp::tick_flicks tk{};
    tk.frames = frames;
    node(tk);
    return l;
  };

  SECTION("Saw ramps up and resets, inside the level set by b")
  {
    const auto l = render("Saw", 2048, 0.05f, 0.5f, 0.5f);
    int resets = 0;
    for(std::size_t i = 1; i < l.size(); i++)
    {
      CHECK(std::abs(l[i]) <= 0.5001);
      if(l[i] < l[i - 1])
        resets++;
    }
    // 20 + 2000 * 0.05 = 120 Hz over 2048 frames at 44100: about 5 cycles.
    CHECK(resets >= 4);
    CHECK(resets <= 7);
  }

  SECTION("Triangle is symmetric and bounded by b")
  {
    const auto l = render("Triangle", 2048, 0.05f, 0.4f, 0.5f);
    double mn = 1e9, mx = -1e9;
    for(double s : l)
    {
      mn = std::min(mn, s);
      mx = std::max(mx, s);
    }
    CHECK(mx == Approx(0.4).epsilon(0.02));
    CHECK(mn == Approx(-0.4).epsilon(0.02));
  }

  SECTION("Pulse only takes two values")
  {
    const auto l = render("Pulse", 512, 0.05f, 0.3f, 0.5f);
    for(double s : l)
      CHECK(std::abs(std::abs(s) - 0.3) < 1e-6);
  }

  SECTION("White noise fills its range without leaving it")
  {
    const auto l = render("White noise", 4096, 0.5f, 0.5f, 0.5f);
    double mn = 1e9, mx = -1e9, sum = 0.;
    for(double s : l)
    {
      CHECK(std::abs(s) <= 0.5001);
      mn = std::min(mn, s);
      mx = std::max(mx, s);
      sum += s;
    }
    CHECK(mx > 0.45);
    CHECK(mn < -0.45);
    CHECK(std::abs(sum / l.size()) < 0.05); // roughly zero-mean
  }

  SECTION("Kick fires straight away and decays")
  {
    const auto l = render("Kick", 8192, 0.15f, 0.3f, 0.8f);
    double head = 0., tail = 0.;
    for(int i = 0; i < 1024; i++)
      head = std::max(head, std::abs(l[i]));
    for(int i = 7168; i < 8192; i++)
      tail = std::max(tail, std::abs(l[i]));
    CHECK(head > 0.3);  // audible from the first block
    CHECK(tail < head); // and it decays
    for(double s : l)
      CHECK(std::abs(s) <= 0.8001);
  }
}

// ===========================================================================
// A lint for the one ExprTK surprise that fails silently
// ===========================================================================

namespace
{
//! ExprTK's implicit multiplication ("2 pi", "(a+b) cos(p)") does *not* apply
//! after a function call or a vector element: `m3[0] (1 - k)` and
//! `if(c, a, b) (x - y)` both compile, and both quietly evaluate to just the
//! left-hand side. Every preset that meant to multiply there has to say so.
//!
//! Returns the offending fragment, or an empty string if the expression is fine.
std::string implicit_multiply_trap(std::string_view e)
{
  const auto ident = [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
           || c == '_';
  };
  const auto next_visible = [&](std::size_t i) {
    while(i < e.size() && (e[i] == ' ' || e[i] == '\t' || e[i] == '\n' || e[i] == '\r'))
      i++;
    return i;
  };
  const auto report = [&](std::size_t at) {
    const auto from = at > 24 ? at - 24 : 0;
    return std::string(e.substr(from, 48));
  };

  std::vector<bool> is_call; // per open paren: was it preceded by an identifier?
  for(std::size_t i = 0; i < e.size(); i++)
  {
    if(e[i] == '(')
    {
      std::size_t j = i;
      while(j > 0 && (e[j - 1] == ' ' || e[j - 1] == '\t' || e[j - 1] == '\n'))
        j--;
      is_call.push_back(j > 0 && ident(e[j - 1]));
    }
    else if(e[i] == ')')
    {
      const bool call = !is_call.empty() && is_call.back();
      if(!is_call.empty())
        is_call.pop_back();
      if(call)
      {
        const auto k = next_visible(i + 1);
        if(k < e.size() && e[k] == '(')
          return report(i);
      }
    }
    else if(e[i] == ']')
    {
      const auto k = next_visible(i + 1);
      if(k < e.size() && e[k] == '(')
        return report(i);
    }
  }
  return {};
}

template <std::size_t N>
void lint(const preset_corpus::entry (&arr)[N], const char* what)
{
  for(const auto& p : arr)
  {
    INFO(what << " / " << p.name << "\n" << p.expr);
    const auto trap = implicit_multiply_trap(p.expr);
    INFO("around: " << trap);
    CHECK(trap.empty());
  }
}
}

TEST_CASE("Presets: no silent implicit-multiplication traps", "[fx][exprtk][presets][lint]")
{
  SECTION("the detector itself")
  {
    CHECK(implicit_multiply_trap("out := 2 pi (1 + a)").empty());
    CHECK(implicit_multiply_trap("out := (1 + a) (2 + b)").empty());
    CHECK(implicit_multiply_trap("out := a (2 + b)").empty());
    CHECK(implicit_multiply_trap("out := sin(a) cos(b)").empty());
    CHECK(implicit_multiply_trap("if(a > b) { c := 1; }").empty());
    CHECK(implicit_multiply_trap("for(var i := 0; i < 4; i += 1) { c := 1; }").empty());

    CHECK_FALSE(implicit_multiply_trap("m3[0] := m3[0] (1 - k)").empty());
    CHECK_FALSE(implicit_multiply_trap("m1 := if(v > m1, at, rl) (v - m1)").empty());
    CHECK_FALSE(implicit_multiply_trap("out := sin(a) (1 + b)").empty());
  }

  lint(preset_corpus::arraygen, "Arraygen");
  lint(preset_corpus::arraymap, "Arraymap");
  lint(preset_corpus::micromap, "Micromap");
  lint(preset_corpus::value_filter, "Expression Value Filter");
  lint(preset_corpus::value_generator, "Expression Value Generator");
  lint(preset_corpus::audio_filter, "Expression Audio Filter");
  lint(preset_corpus::audio_generator, "Expression Audio Generator");
}

// ===========================================================================
// Menu categories
// ===========================================================================

namespace
{
template <std::size_t N>
void check_categories(
    const preset_corpus::entry (&arr)[N], const char* what,
    std::vector<std::string>& seen)
{
  for(const auto& p : arr)
  {
    INFO(what << " / " << p.name);
    const std::string_view cat{p.category};

    // A preset with no category lands loose in the root of the preset menu,
    // which is unusable once a process has dozens of them.
    CHECK_FALSE(cat.empty());
    // "/" nests submenus: no empty or untrimmed path component.
    CHECK(cat.front() != '/');
    CHECK(cat.back() != '/');
    CHECK(cat.find("//") == std::string_view::npos);
    CHECK(cat.front() != ' ');
    CHECK(cat.back() != ' ');

    seen.push_back(std::string(what) + " / " + p.category);
  }
}
}

TEST_CASE("Presets: every preset is filed under a category", "[fx][exprtk][presets][lint]")
{
  std::vector<std::string> seen;
  check_categories(preset_corpus::arraygen, "Arraygen", seen);
  check_categories(preset_corpus::arraymap, "Arraymap", seen);
  check_categories(preset_corpus::micromap, "Micromap", seen);
  check_categories(preset_corpus::value_filter, "Expression Value Filter", seen);
  check_categories(preset_corpus::value_generator, "Expression Value Generator", seen);
  check_categories(preset_corpus::audio_filter, "Expression Audio Filter", seen);
  check_categories(preset_corpus::audio_generator, "Expression Audio Generator", seen);

  // A category holding a single preset is a submenu with one entry in it:
  // usually a typo rather than a deliberate grouping. These five were
  // reviewed and are deliberate ("Noise gate" really is Dynamics; each name
  // matches the same category in another process's menu); a singleton not on
  // this list needs the same review.
  static const std::vector<std::string> reviewed_singletons{
      "Arraygen / Colour",
      "Arraymap / Geometry",
      "Arraymap / Shaping",
      "Expression Audio Filter / Dynamics",
      "Expression Value Filter / Random",
  };
  std::sort(seen.begin(), seen.end());
  for(std::size_t i = 0; i < seen.size(); i++)
  {
    const auto count = std::count(seen.begin(), seen.end(), seen[i]);
    INFO(seen[i] << ": " << count << " preset(s)");
    const bool reviewed
        = std::find(reviewed_singletons.begin(), reviewed_singletons.end(), seen[i])
          != reviewed_singletons.end();
    CHECK((count >= 2 || reviewed));
  }

  // Sanity: the corpus really did pick up every folder of the library.
  CHECK(std::size(preset_corpus::arraygen) >= 30);
  CHECK(std::size(preset_corpus::micromap) >= 50);
  CHECK(std::size(preset_corpus::value_generator) >= 30);
}
