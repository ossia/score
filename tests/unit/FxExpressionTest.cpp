// The value-domain ExprTK processes of score_plugin_fx:
//   Micromap, Expression Value Filter, Expression Value Generator,
//   Arraymap, Arraygen, Expression Audio Generator.
//
// These all share GenericMathMapping (Fx/MathMapping_generic.hpp), which is
// where the interesting behaviour lives: how an expression's result is turned
// into an ossia::value, when a polyphonic node instantiates one expression per
// element instead of re-evaluating a single one, and how the ExprTK vector
// views are re-pointed when the input size changes.
//
// The expression bodies exercised here are, wherever possible, the ones that
// actually ship in packages/default/Presets.
#include <Fx/Arraygen.hpp>
#include <Fx/Arraymap.hpp>
#include <Fx/MathAudioGenerator.hpp>
#include <Fx/MathGenerator.hpp>
#include <Fx/MathValueFilter.hpp>
#include <Fx/MicroMapping.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <vector>

using Catch::Approx;

namespace
{
// ---------------------------------------------------------------------------
// Harness
// ---------------------------------------------------------------------------

//! Collects everything a node pushes to its value output.
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

  const ossia::value& last() const
  {
    REQUIRE(!values.empty());
    return values.back();
  }
  void clear() { values.clear(); }
};

halp::tick_flicks
make_tick(int64_t start, int64_t end, double relpos = 0., int64_t parent_dur = 1000000)
{
  halp::tick_flicks tk{};
  tk.frames = 64;
  tk.start_in_flicks = start;
  tk.end_in_flicks = end;
  tk.relative_position = relpos;
  tk.parent_duration = parent_dur;
  return tk;
}

float as_float(const ossia::value& v)
{
  INFO("value type: " << (int)v.get_type());
  REQUIRE(v.get_type() == ossia::val_type::FLOAT);
  return *v.target<float>();
}

std::vector<ossia::value> as_list(const ossia::value& v)
{
  INFO("value type: " << (int)v.get_type());
  REQUIRE(v.get_type() == ossia::val_type::LIST);
  return *v.target<std::vector<ossia::value>>();
}

ossia::vec2f as_vec2(const ossia::value& v)
{
  INFO("value type: " << (int)v.get_type());
  REQUIRE(v.get_type() == ossia::val_type::VEC2F);
  return *v.target<ossia::vec2f>();
}

//! Drives a node that has one value inlet + one value outlet, plus a trigger.
template <typename Node>
struct triggered_harness
{
  Node node;
  value_sink out;

  triggered_harness()
  {
    out.wire(node);
    if constexpr(requires { node.prepare(halp::setup{}); })
      node.prepare(halp::setup{
          .input_channels = 0, .output_channels = 0, .frames = 64, .rate = 44100.});
  }

  void set(const std::string& e) { node.inputs.expr.value = e; }

  void send(const ossia::value& v, halp::tick_flicks tk)
  {
    node.inputs.port.value = v;
    node.trigger = true;
    node(tk);
  }
  void send(const ossia::value& v, int64_t t = 0, double pos = 0.)
  {
    send(v, make_tick(t, t + 1000, pos));
  }
};

using micromap_harness = triggered_harness<Nodes::MicroMapping::Node>;
using valuefilter_harness = triggered_harness<Nodes::MathMapping::Node>;
using arraymap_harness = triggered_harness<Nodes::ArrayMapping::Node>;

//! Drives a generator node (no inlet: it fires on every tick).
template <typename Node>
struct generator_harness
{
  Node node;
  value_sink out;

  generator_harness() { out.wire(node); }

  void set(const std::string& e) { node.inputs.expr.value = e; }
  void run(halp::tick_flicks tk) { node(tk); }
  void run(int64_t t = 0, double pos = 0.) { node(make_tick(t, t + 1000, pos)); }
};

using valuegen_harness = generator_harness<Nodes::MathGenerator::Node>;

struct arraygen_harness : generator_harness<Nodes::ArrayGenerator::Node>
{
  void size(int n) { node.inputs.sz.value = n; }
};
}

// ===========================================================================
// Micromap
// ===========================================================================

TEST_CASE("Micromap: the default expression scales the input", "[fx][exprtk][micromap]")
{
  micromap_harness h;
  h.set("x / 127");
  h.send(127.f);
  CHECK(as_float(h.out.last()) == Approx(1.));

  h.send(0.f);
  CHECK(as_float(h.out.last()) == Approx(0.));

  h.send(64.f);
  CHECK(as_float(h.out.last()) == Approx(64. / 127.));
}

TEST_CASE("Micromap: shipped scalar presets", "[fx][exprtk][micromap][presets]")
{
  struct
  {
    const char* expr;
    float in;
    float expected;
  } cases[]{
      {"abs(x)", -3.f, 3.f},
      {"ceil(x)", 1.2f, 2.f},
      {"floor(x)", 1.8f, 1.f},
      {"round(x)", 1.5f, 2.f},
      {"x*x", 3.f, 9.f},
      {"x*x*x", 3.f, 27.f},
      {"sqrt(x)", 16.f, 4.f},
      {"x / 16\n", 32.f, 2.f},
      {"x / 127", 127.f, 1.f},
      {"deg2rad(x)", 180.f, 3.14159265f},
      {"rad2deg(x)", 3.14159265f, 180.f},
      {"(440 pow(2, (x - 69)/ 12))", 69.f, 440.f},
      {"1 /  (440 pow(2, (x - 69)/ 12))", 69.f, 1.f / 440.f},
      {"abs((1 - x) - 17)", 0.f, 16.f},
  };

  for(const auto& c : cases)
  {
    INFO(c.expr);
    micromap_harness h;
    h.set(c.expr);
    h.send(c.in);
    CHECK(as_float(h.out.last()) == Approx(c.expected).epsilon(1e-5));
  }
}

TEST_CASE("Micromap: px is the previous input", "[fx][exprtk][micromap]")
{
  micromap_harness h;
  h.set("x - px"); // shipped "Derivative" preset
  h.send(1.f);
  CHECK(as_float(h.out.last()) == Approx(1.));
  h.send(3.f);
  CHECK(as_float(h.out.last()) == Approx(2.));
  h.send(2.5f);
  CHECK(as_float(h.out.last()) == Approx(-0.5));
}

TEST_CASE("Micromap: po is the previous output", "[fx][exprtk][micromap]")
{
  micromap_harness h;
  h.set("po + x");
  h.send(1.f);
  CHECK(as_float(h.out.last()) == Approx(1.));
  h.send(1.f);
  CHECK(as_float(h.out.last()) == Approx(2.));
  h.send(2.f);
  CHECK(as_float(h.out.last()) == Approx(4.));
}

TEST_CASE("Micromap: xv indexes an array input", "[fx][exprtk][micromap][array]")
{
  // The shipped "X / Y / Z coord" presets.
  const ossia::value in = ossia::vec3f{10.f, 20.f, 30.f};

  {
    micromap_harness h;
    h.set("xv[0]");
    h.send(in);
    CHECK(as_float(h.out.last()) == Approx(10.));
  }
  {
    micromap_harness h;
    h.set("xv[1]");
    h.send(in);
    CHECK(as_float(h.out.last()) == Approx(20.));
  }
  {
    micromap_harness h;
    h.set("xv[2]");
    h.send(in);
    CHECK(as_float(h.out.last()) == Approx(30.));
  }
}

TEST_CASE("Micromap: xv[] reports the input size", "[fx][exprtk][micromap][array]")
{
  micromap_harness h;
  h.set("xv[]");

  h.send(ossia::value{std::vector<ossia::value>{1.f, 2.f, 3.f}});
  CHECK(as_float(h.out.last()) == Approx(3.));

  h.send(ossia::value{std::vector<ossia::value>{1.f, 2.f, 3.f, 4.f, 5.f}});
  CHECK(as_float(h.out.last()) == Approx(5.));

  h.send(ossia::vec2f{1.f, 2.f});
  CHECK(as_float(h.out.last()) == Approx(2.));
}

TEST_CASE("Micromap: an array expression follows a changing input size", "[fx][exprtk][micromap][array]")
{
  micromap_harness h;
  h.set("var acc := 0; for(var i := 0; i < xv[]; i += 1) { acc += xv[i]; }; acc");

  h.send(ossia::value{std::vector<ossia::value>{1.f, 2.f, 3.f}});
  CHECK(as_float(h.out.last()) == Approx(6.));

  h.send(ossia::value{std::vector<ossia::value>{10.f, 20.f}});
  CHECK(as_float(h.out.last()) == Approx(30.));

  h.send(ossia::value{std::vector<ossia::value>{1.f, 1.f, 1.f, 1.f, 1.f, 1.f}});
  CHECK(as_float(h.out.last()) == Approx(6.));

  // ... and back to a size it already used
  h.send(ossia::value{std::vector<ossia::value>{5.f, 5.f, 5.f}});
  CHECK(as_float(h.out.last()) == Approx(15.));
}

TEST_CASE("Micromap: pxv is the previous array input", "[fx][exprtk][micromap][array]")
{
  micromap_harness h;
  h.set("xv[0] - pxv[0]");

  h.send(ossia::vec2f{5.f, 0.f});
  CHECK(as_float(h.out.last()) == Approx(5.));
  h.send(ossia::vec2f{8.f, 0.f});
  CHECK(as_float(h.out.last()) == Approx(3.));
  h.send(ossia::vec2f{2.f, 0.f});
  CHECK(as_float(h.out.last()) == Approx(-6.));
}

TEST_CASE("Micromap: an index that a smaller input cannot satisfy is not fatal", "[fx][exprtk][micromap][array]")
{
  // "Z coord" (xv[2]) fed a scalar cannot be evaluated: the ExprTK vector view
  // is one element wide. The node must simply produce nothing for that cycle
  // and recover as soon as a wide-enough input arrives, rather than wedging
  // itself for the rest of the session.
  micromap_harness h;
  h.set("xv[2]");

  h.send(0.5f);
  const auto after_scalar = h.out.values.size();

  h.send(ossia::vec3f{1.f, 2.f, 3.f});
  REQUIRE(h.out.values.size() > after_scalar);
  CHECK(as_float(h.out.last()) == Approx(3.));
}

TEST_CASE("Micromap: an invalid expression produces no output", "[fx][exprtk][micromap][error]")
{
  micromap_harness h;
  h.set("1 +");
  h.send(1.f);
  CHECK(h.out.values.empty());

  // ... and the node recovers once the expression is fixed.
  h.set("x * 2");
  h.send(21.f);
  CHECK(as_float(h.out.last()) == Approx(42.));
}

TEST_CASE("Micromap: an untriggered tick produces nothing", "[fx][exprtk][micromap]")
{
  micromap_harness h;
  h.set("x * 2");
  h.node.inputs.port.value = 1.f;
  h.node(make_tick(0, 1000));
  CHECK(h.out.values.empty());
}

TEST_CASE("Micromap: integer, bool and string inputs are converted", "[fx][exprtk][micromap]")
{
  micromap_harness h;
  h.set("x * 2");

  h.send(21);
  CHECK(as_float(h.out.last()) == Approx(42.));

  h.send(true);
  CHECK(as_float(h.out.last()) == Approx(2.));

  h.send(false);
  CHECK(as_float(h.out.last()) == Approx(0.));
}

// ===========================================================================
// Expression Value Filter (MathMapping)
// ===========================================================================

TEST_CASE("Expression Value Filter: a/b/c parameters", "[fx][exprtk][valuefilter]")
{
  valuefilter_harness h;
  h.set("x * a + b * 10 + c * 100");
  h.node.inputs.a.value = 2.f;
  h.node.inputs.b.value = 0.5f;
  h.node.inputs.c.value = 0.25f;
  h.send(3.f);
  CHECK(as_float(h.out.last()) == Approx(3. * 2. + 5. + 25.));
}

TEST_CASE("Expression Value Filter: pa/pb/pc are the previous parameters", "[fx][exprtk][valuefilter]")
{
  valuefilter_harness h;
  h.set("pa");
  h.node.inputs.a.value = 1.f;
  h.send(0.f);
  CHECK(as_float(h.out.last()) == Approx(0.)); // no previous tick yet

  h.node.inputs.a.value = 5.f;
  h.send(0.f);
  CHECK(as_float(h.out.last()) == Approx(1.));
}

TEST_CASE("Expression Value Filter: t, dt and pos come from the tick", "[fx][exprtk][valuefilter]")
{
  {
    valuefilter_harness h;
    h.set("t");
    h.send(0.f, make_tick(1234, 2000, 0.));
    CHECK(as_float(h.out.last()) == Approx(1234.));
  }
  {
    valuefilter_harness h;
    h.set("pos");
    h.send(0.f, make_tick(0, 1000, 0.42));
    CHECK(as_float(h.out.last()) == Approx(0.42));
  }
  {
    valuefilter_harness h;
    h.set("dt");
    h.send(0.f, make_tick(1000, 2000, 0.));
    CHECK(as_float(h.out.last()) == Approx(1000.));
    h.send(0.f, make_tick(2500, 3000, 0.));
    CHECK(as_float(h.out.last()) == Approx(1500.));
  }
}

TEST_CASE("Expression Value Filter: fs is the sample rate", "[fx][exprtk][valuefilter]")
{
  valuefilter_harness h;
  h.set("fs");
  h.send(0.f);
  CHECK(as_float(h.out.last()) == Approx(44100.));
}

TEST_CASE("Expression Value Filter: m1..m3 persist between ticks", "[fx][exprtk][valuefilter]")
{
  // Shipped "Increment-on-press" preset.
  valuefilter_harness h;
  h.set("m1 := m1 + if(x == 1 and x != px, 1, 0)");

  h.send(0.f);
  CHECK(as_float(h.out.last()) == Approx(0.));
  h.send(1.f);
  CHECK(as_float(h.out.last()) == Approx(1.));
  h.send(1.f);
  CHECK(as_float(h.out.last()) == Approx(1.)); // still held
  h.send(0.f);
  h.send(1.f);
  CHECK(as_float(h.out.last()) == Approx(2.));
}

TEST_CASE("Expression Value Filter: Sample and Hold A", "[fx][exprtk][valuefilter][presets]")
{
  valuefilter_harness h;
  h.set("m2 := if(a >= 1.0 and m1 < 1.0, x, m2);\nm1 := a;\nm2\n");

  h.node.inputs.a.value = 0.f;
  h.send(10.f);
  CHECK(as_float(h.out.last()) == Approx(0.));

  h.node.inputs.a.value = 1.f; // rising edge: sample
  h.send(10.f);
  CHECK(as_float(h.out.last()) == Approx(10.));

  h.send(99.f); // held: a is still 1, no new edge
  CHECK(as_float(h.out.last()) == Approx(10.));

  h.node.inputs.a.value = 0.f;
  h.send(99.f);
  CHECK(as_float(h.out.last()) == Approx(10.));

  h.node.inputs.a.value = 1.f; // new edge
  h.send(99.f);
  CHECK(as_float(h.out.last()) == Approx(99.));
}

TEST_CASE("Expression Value Filter: Smoothstep", "[fx][exprtk][valuefilter][presets]")
{
  valuefilter_harness h;
  h.set("var l := a; var r := b;\nvar v := (clamp(x, l, r) - l) / (r - l);\nv * v * (3 - 2 * v)");
  h.node.inputs.a.value = 0.f;
  h.node.inputs.b.value = 1.f;

  h.send(0.f);
  CHECK(as_float(h.out.last()) == Approx(0.));
  h.send(0.5f);
  CHECK(as_float(h.out.last()) == Approx(0.5));
  h.send(1.f);
  CHECK(as_float(h.out.last()) == Approx(1.));
  h.send(-5.f);
  CHECK(as_float(h.out.last()) == Approx(0.));
}

TEST_CASE("Expression Value Filter: Direction change", "[fx][exprtk][valuefilter][presets]")
{
  valuefilter_harness h;
  h.set("if(abs(x - px) < 0.1, 0,  if(px< x, 1, -1))");

  h.send(0.f);
  CHECK(as_float(h.out.last()) == Approx(0.));
  h.send(1.f);
  CHECK(as_float(h.out.last()) == Approx(1.));
  h.send(0.f);
  CHECK(as_float(h.out.last()) == Approx(-1.));
  h.send(0.05f);
  CHECK(as_float(h.out.last()) == Approx(0.));
}

TEST_CASE("Expression Value Filter: Exponential Average converges", "[fx][exprtk][valuefilter][presets]")
{
  valuefilter_harness h;
  h.set("m1 += 1;\nm2 := m2 + (x - m2) / min(m1, (0.01 + a) * 100);");
  h.node.inputs.a.value = 0.1f;

  for(int i = 0; i < 200; i++)
    h.send(1.f);
  CHECK(as_float(h.out.last()) == Approx(1.).epsilon(0.05));
}

TEST_CASE("Expression Value Filter: Noisify stays around the input", "[fx][exprtk][valuefilter][presets]")
{
  valuefilter_harness h;
  h.set("var rnd_m := pow(2, 31);\nvar rnd_a := 1103515245;\nvar rnd_c := 12345;\n"
        "if(m2 == 0) {\n  m2 := 1;\n  m1 := 12345678;\n}\n"
        "var r := (rnd_a * m1 + rnd_c) % rnd_m;\nm1 := r;\n"
        "x + ((a + b * 10 + c * 100) * (0.5 - r / (2^31)));");
  h.node.inputs.a.value = 0.25f;
  h.node.inputs.b.value = 0.f;
  h.node.inputs.c.value = 0.f;

  for(int i = 0; i < 32; i++)
  {
    h.send(1.f);
    const auto v = as_float(h.out.last());
    CHECK(v > 0.8f);
    CHECK(v < 1.2f);
  }
}

TEST_CASE("Expression Value Filter: return produces a vector output", "[fx][exprtk][valuefilter]")
{
  valuefilter_harness h;
  h.set("return [x, x * 2, x * 3]");
  h.send(1.f);
  auto v = h.out.last();
  REQUIRE(v.get_type() == ossia::val_type::VEC3F);
  CHECK((*v.target<ossia::vec3f>())[2] == Approx(3.f));
}

TEST_CASE("Expression Value Filter: the array path", "[fx][exprtk][valuefilter][array]")
{
  valuefilter_harness h;
  h.set("xv[0] + xv[1]");
  h.send(ossia::vec2f{3.f, 4.f});
  CHECK(as_float(h.out.last()) == Approx(7.));
}

// ===========================================================================
// Expression Value Generator (MathGenerator)
// ===========================================================================

TEST_CASE("Expression Value Generator: pos and t come from the tick", "[fx][exprtk][valuegen]")
{
  {
    valuegen_harness h;
    h.set("pos");
    h.run(make_tick(0, 1000, 0.33));
    CHECK(as_float(h.out.last()) == Approx(0.33));
  }
  {
    valuegen_harness h;
    h.set("t");
    h.run(make_tick(0, 4242, 0.));
    CHECK(as_float(h.out.last()) == Approx(4242.));
  }
  {
    // pos is only meaningful inside a parent with a duration
    valuegen_harness h;
    h.set("pos");
    h.run(make_tick(0, 1000, 0.33, 0));
    CHECK(as_float(h.out.last()) == Approx(0.));
  }
}

TEST_CASE("Expression Value Generator: XY / XYZ presets", "[fx][exprtk][valuegen][presets]")
{
  {
    valuegen_harness h;
    h.set("return [a, b];");
    h.node.inputs.a.value = 0.25f;
    h.node.inputs.b.value = 0.75f;
    h.run();
    auto v = as_vec2(h.out.last());
    CHECK(v[0] == Approx(0.25f));
    CHECK(v[1] == Approx(0.75f));
  }
  {
    valuegen_harness h;
    h.set("return [a, b, c];");
    h.node.inputs.c.value = 0.5f;
    h.run();
    auto v = h.out.last();
    REQUIRE(v.get_type() == ossia::val_type::VEC3F);
    CHECK((*v.target<ossia::vec3f>())[2] == Approx(0.5f));
  }
}

TEST_CASE("Expression Value Generator: Random Color stays in range", "[fx][exprtk][valuegen][presets]")
{
  valuegen_harness h;
  h.set("return [\n  random(0, a), \n  random(0, b),\n  random(0, c), \n  1\n]");
  h.node.inputs.a.value = 1.f;
  h.node.inputs.b.value = 1.f;
  h.node.inputs.c.value = 1.f;

  for(int i = 0; i < 32; i++)
  {
    h.run(i * 1000, i / 32.);
    auto v = h.out.last();
    REQUIRE(v.get_type() == ossia::val_type::VEC4F);
    auto& vec = *v.target<ossia::vec4f>();
    for(int k = 0; k < 3; k++)
    {
      CHECK(vec[k] >= 0.f);
      CHECK(vec[k] <= 1.f);
    }
    CHECK(vec[3] == Approx(1.f));
  }
}

TEST_CASE("Expression Value Generator: Perlin / noise presets stay bounded", "[fx][exprtk][valuegen][presets]")
{
  valuegen_harness h;
  h.set("noise(pos * a * 10, b * 10, c)  * 100");
  h.node.inputs.a.value = 1.f;
  h.node.inputs.b.value = 0.3f;
  h.node.inputs.c.value = 0.5f;

  for(int i = 0; i <= 64; i++)
  {
    h.run(i * 1000, i / 64.);
    const auto v = as_float(h.out.last());
    CHECK(v >= 0.f);
    CHECK(v <= 100.f);
  }
}

TEST_CASE("Expression Value Generator: Logistic preset", "[fx][exprtk][valuegen][presets]")
{
  valuegen_harness h;
  h.set("if(m1 == 0) {\n  m2 := 0.8;\n  m1 := 1;\n}\n\nvar r := 4 * a;\nm2 := r * m2 * (1 - m2);\nm2;");
  h.node.inputs.a.value = 0.9f;

  for(int i = 0; i < 100; i++)
  {
    h.run(i * 1000, i / 100.);
    const auto v = as_float(h.out.last());
    CHECK(v >= 0.f);
    CHECK(v <= 1.f);
  }
}

TEST_CASE("Expression Value Generator: po is the previous scalar output", "[fx][exprtk][valuegen]")
{
  valuegen_harness h;
  h.set("po + 1");
  h.run();
  CHECK(as_float(h.out.last()) == Approx(1.));
  h.run();
  CHECK(as_float(h.out.last()) == Approx(2.));
  h.run();
  CHECK(as_float(h.out.last()) == Approx(3.));
}

TEST_CASE("Expression Value Generator: pov is the previous vector output", "[fx][exprtk][valuegen]")
{
  valuegen_harness h;
  h.set("return [pov[0] + 1, pov[1] + 2]");

  h.run();
  {
    auto v = as_vec2(h.out.last());
    CHECK(v[0] == Approx(1.f));
    CHECK(v[1] == Approx(2.f));
  }
  h.run();
  {
    auto v = as_vec2(h.out.last());
    CHECK(v[0] == Approx(2.f));
    CHECK(v[1] == Approx(4.f));
  }
}

TEST_CASE("Expression Value Generator: a very long returned list does not corrupt pov", "[fx][exprtk][valuegen]")
{
  // pov is an ExprTK view onto a fixed-capacity buffer: feeding back a list
  // longer than that buffer must not re-point it at freed memory.
  valuegen_harness h;
  h.set("var v[2048] := [1];\nfor(var i := 0; i < 2048; i += 1) { v[i] := i; };\nreturn [v]");
  h.run();
  h.run();
  const auto outer = as_list(h.out.last());
  REQUIRE(outer.size() == 1);
  CHECK(as_list(outer[0]).size() == 2048);
}

TEST_CASE("Expression Value Generator: an invalid expression produces no output", "[fx][exprtk][valuegen][error]")
{
  valuegen_harness h;
  h.set("cos(");
  h.run();
  CHECK(h.out.values.empty());

  h.set("cos(0)");
  h.run();
  CHECK(as_float(h.out.last()) == Approx(1.));
}

// ===========================================================================
// Arraymap
// ===========================================================================

TEST_CASE("Arraymap: applies the expression to every element", "[fx][exprtk][arraymap]")
{
  arraymap_harness h;
  h.set("x * 2");
  h.send(ossia::value{std::vector<ossia::value>{1.f, 2.f, 3.f}});

  const auto l = as_list(h.out.last());
  REQUIRE(l.size() == 3);
  CHECK(as_float(l[0]) == Approx(2.));
  CHECK(as_float(l[1]) == Approx(4.));
  CHECK(as_float(l[2]) == Approx(6.));
}

TEST_CASE("Arraymap: i is the element index and n the element count", "[fx][exprtk][arraymap]")
{
  {
    arraymap_harness h;
    h.set("i");
    h.send(ossia::value{std::vector<ossia::value>{0.f, 0.f, 0.f, 0.f}});
    const auto l = as_list(h.out.last());
    REQUIRE(l.size() == 4);
    for(int i = 0; i < 4; i++)
      CHECK(as_float(l[i]) == Approx(i));
  }
  {
    arraymap_harness h;
    h.set("n");
    h.send(ossia::value{std::vector<ossia::value>{0.f, 0.f, 0.f}});
    const auto l = as_list(h.out.last());
    REQUIRE(l.size() == 3);
    for(auto& v : l)
      CHECK(as_float(v) == Approx(3.));
  }
}

TEST_CASE("Arraymap: vec inputs are mapped element-wise", "[fx][exprtk][arraymap]")
{
  {
    arraymap_harness h;
    h.set("x + 1");
    h.send(ossia::vec2f{1.f, 2.f});
    const auto l = as_list(h.out.last());
    REQUIRE(l.size() == 2);
    CHECK(as_float(l[1]) == Approx(3.));
  }
  {
    arraymap_harness h;
    h.set("x + 1");
    h.send(ossia::vec4f{1.f, 2.f, 3.f, 4.f});
    const auto l = as_list(h.out.last());
    REQUIRE(l.size() == 4);
    CHECK(as_float(l[3]) == Approx(5.));
  }
}

TEST_CASE("Arraymap: each element keeps its own po", "[fx][exprtk][arraymap]")
{
  arraymap_harness h;
  h.set("po + x");
  const ossia::value in{std::vector<ossia::value>{1.f, 10.f, 100.f}};

  h.send(in);
  {
    const auto l = as_list(h.out.last());
    CHECK(as_float(l[0]) == Approx(1.));
    CHECK(as_float(l[2]) == Approx(100.));
  }
  h.send(in);
  {
    const auto l = as_list(h.out.last());
    CHECK(as_float(l[0]) == Approx(2.));
    CHECK(as_float(l[1]) == Approx(20.));
    CHECK(as_float(l[2]) == Approx(200.));
  }
}

TEST_CASE("Arraymap: each element keeps its own px", "[fx][exprtk][arraymap]")
{
  arraymap_harness h;
  h.set("x - px");
  h.send(ossia::value{std::vector<ossia::value>{1.f, 2.f}});
  h.send(ossia::value{std::vector<ossia::value>{5.f, 20.f}});
  const auto l = as_list(h.out.last());
  REQUIRE(l.size() == 2);
  CHECK(as_float(l[0]) == Approx(4.));
  CHECK(as_float(l[1]) == Approx(18.));
}

TEST_CASE("Arraymap: an element expression may itself return a point", "[fx][exprtk][arraymap]")
{
  arraymap_harness h;
  h.set("return [x, x * 2]");
  h.send(ossia::value{std::vector<ossia::value>{1.f, 3.f}});

  const auto l = as_list(h.out.last());
  REQUIRE(l.size() == 2);
  auto p0 = as_vec2(l[0]);
  auto p1 = as_vec2(l[1]);
  CHECK(p0[0] == Approx(1.f));
  CHECK(p0[1] == Approx(2.f));
  CHECK(p1[0] == Approx(3.f));
  CHECK(p1[1] == Approx(6.f));
}

TEST_CASE("Arraymap: a point-returning element expression that also uses po", "[fx][exprtk][arraymap]")
{
  arraymap_harness h;
  h.set("var y := po; return [x, y]");
  h.send(ossia::value{std::vector<ossia::value>{1.f, 2.f}});
  {
    const auto l = as_list(h.out.last());
    REQUIRE(l.size() == 2);
    CHECK(as_vec2(l[0])[0] == Approx(1.f));
  }
}

TEST_CASE("Arraymap: shipped noise presets stay near the input", "[fx][exprtk][arraymap][presets]")
{
  arraymap_harness h;
  h.set("x + 0.1noise(0.00000001t + 10i, 3, 0.5)");
  h.send(ossia::value{std::vector<ossia::value>{0.f, 0.f, 0.f, 0.f}}, 1000000);

  const auto l = as_list(h.out.last());
  REQUIRE(l.size() == 4);
  for(auto& v : l)
  {
    CHECK(as_float(v) >= -0.001f);
    CHECK(as_float(v) <= 0.101f);
  }
}

TEST_CASE("Arraymap: the element count follows the input", "[fx][exprtk][arraymap]")
{
  arraymap_harness h;
  h.set("x * 2");

  h.send(ossia::value{std::vector<ossia::value>{1.f, 2.f, 3.f}});
  CHECK(as_list(h.out.last()).size() == 3);

  h.send(ossia::value{std::vector<ossia::value>{1.f}});
  CHECK(as_list(h.out.last()).size() == 1);

  h.send(ossia::value{std::vector<ossia::value>{1.f, 2.f, 3.f, 4.f, 5.f}});
  CHECK(as_list(h.out.last()).size() == 5);
}

TEST_CASE("Arraymap: changing the expression takes effect", "[fx][exprtk][arraymap]")
{
  arraymap_harness h;
  const ossia::value in{std::vector<ossia::value>{1.f, 2.f}};

  h.set("x * 2");
  h.send(in);
  CHECK(as_float(as_list(h.out.last())[1]) == Approx(4.));

  h.set("x * 10");
  h.send(in);
  CHECK(as_float(as_list(h.out.last())[1]) == Approx(20.));

  h.set("x + 1");
  h.send(in);
  CHECK(as_float(as_list(h.out.last())[1]) == Approx(3.));
}

// ===========================================================================
// Arraygen
// ===========================================================================

TEST_CASE("Arraygen: a scalar expression fills the array", "[fx][exprtk][arraygen]")
{
  arraygen_harness h;
  h.size(4);
  h.set("i * 10");
  h.run();

  const auto l = as_list(h.out.last());
  REQUIRE(l.size() == 4);
  for(int i = 0; i < 4; i++)
    CHECK(as_float(l[i]) == Approx(i * 10));
}

TEST_CASE("Arraygen: n is the array size", "[fx][exprtk][arraygen]")
{
  arraygen_harness h;
  h.size(7);
  h.set("n");
  h.run();

  const auto l = as_list(h.out.last());
  REQUIRE(l.size() == 7);
  for(auto& v : l)
    CHECK(as_float(v) == Approx(7.));
}

TEST_CASE("Arraygen: return of a constant", "[fx][exprtk][arraygen][return]")
{
  arraygen_harness h;
  h.size(3);
  h.set("return [ 123 ]");
  h.run();

  const auto l = as_list(h.out.last());
  REQUIRE(l.size() == 3);
  for(auto& v : l)
  {
    const auto inner = as_list(v);
    REQUIRE(inner.size() == 1);
    CHECK(as_float(inner[0]) == Approx(123.));
  }
}

TEST_CASE("Arraygen: return of a registered variable", "[fx][exprtk][arraygen][return]")
{
  arraygen_harness h;
  h.size(3);
  h.set("return [ pos ]");
  h.run(make_tick(0, 1000, 0.5));

  const auto l = as_list(h.out.last());
  REQUIRE(l.size() == 3);
  for(auto& v : l)
  {
    const auto inner = as_list(v);
    REQUIRE(inner.size() == 1);
    CHECK(as_float(inner[0]) == Approx(0.5));
  }
}

TEST_CASE("Arraygen: return of a local variable", "[fx][exprtk][arraygen][return]")
{
  arraygen_harness h;
  h.size(3);
  h.set("var p := 123; return [ p ]");
  h.run();

  const auto l = as_list(h.out.last());
  REQUIRE(l.size() == 3);
  for(auto& v : l)
    CHECK(as_float(as_list(v)[0]) == Approx(123.));
}

TEST_CASE("Arraygen: a local variable assigned from pos, then return", "[fx][exprtk][arraygen][return]")
{
  // Reported bug: `var rrr := pos; return [ pos ]` produced an array of zeroes
  // while `return [ pos ]` and `var p := 123; return [ p ]` both worked. The
  // node was picking the per-element-state code path because the expression
  // contained ":=" and the *substring* "po" (inside "pos").
  arraygen_harness h;
  h.size(3);
  h.set("var rrr := pos; return [ pos ]");
  h.run(make_tick(0, 1000, 0.5));

  const auto l = as_list(h.out.last());
  REQUIRE(l.size() == 3);
  for(auto& v : l)
  {
    const auto inner = as_list(v);
    REQUIRE(inner.size() == 1);
    CHECK(as_float(inner[0]) == Approx(0.5));
  }
}

TEST_CASE("Arraygen: identifiers that merely start with 'po'", "[fx][exprtk][arraygen][return]")
{
  // "pow" and "pos" both contain "po": neither means the expression wants
  // per-element output feedback.
  {
    arraygen_harness h;
    h.size(3);
    h.set("var y := pow(2, i); return [ y ]");
    h.run();
    const auto l = as_list(h.out.last());
    REQUIRE(l.size() == 3);
    CHECK(as_float(as_list(l[0])[0]) == Approx(1.));
    CHECK(as_float(as_list(l[1])[0]) == Approx(2.));
    CHECK(as_float(as_list(l[2])[0]) == Approx(4.));
  }
  {
    arraygen_harness h;
    h.size(2);
    h.set("var q := pos; return [ q, i ]");
    h.run(make_tick(0, 1000, 0.25));
    const auto l = as_list(h.out.last());
    REQUIRE(l.size() == 2);
    CHECK(as_vec2(l[0])[0] == Approx(0.25f));
    CHECK(as_vec2(l[1])[1] == Approx(1.f));
  }
}

TEST_CASE("Arraygen: po really is per-element feedback", "[fx][exprtk][arraygen]")
{
  arraygen_harness h;
  h.size(3);
  h.set("po + 1 + i");

  h.run();
  {
    const auto l = as_list(h.out.last());
    REQUIRE(l.size() == 3);
    CHECK(as_float(l[0]) == Approx(1.));
    CHECK(as_float(l[1]) == Approx(2.));
    CHECK(as_float(l[2]) == Approx(3.));
  }
  h.run();
  {
    const auto l = as_list(h.out.last());
    CHECK(as_float(l[0]) == Approx(2.));
    CHECK(as_float(l[1]) == Approx(4.));
    CHECK(as_float(l[2]) == Approx(6.));
  }
}

TEST_CASE("Arraygen: po feedback with a returned point", "[fx][exprtk][arraygen][return]")
{
  arraygen_harness h;
  h.size(2);
  h.set("var y := po; return [ y, i ]");

  h.run();
  {
    const auto l = as_list(h.out.last());
    REQUIRE(l.size() == 2);
    CHECK(as_vec2(l[0])[1] == Approx(0.f));
    CHECK(as_vec2(l[1])[1] == Approx(1.f));
  }
}

TEST_CASE("Arraygen: returned pairs for every element", "[fx][exprtk][arraygen][return]")
{
  arraygen_harness h;
  h.size(4);
  h.set("return [i, i * i]");
  h.run();

  const auto l = as_list(h.out.last());
  REQUIRE(l.size() == 4);
  for(int i = 0; i < 4; i++)
  {
    auto p = as_vec2(l[i]);
    CHECK(p[0] == Approx((float)i));
    CHECK(p[1] == Approx((float)(i * i)));
  }
}

TEST_CASE("Arraygen: shipped presets produce one point per element", "[fx][exprtk][arraygen][presets]")
{
  struct preset
  {
    const char* expr;
    int size;
    // "Hypo 1" divides by b, which is 0 for the first element: the preset
    // itself yields a NaN point there. Only the shape of the output is checked
    // for it.
    bool finite;
  };
  const std::vector<preset> presets{
      {"var a := 100.1 / (1+i);\nvar b := 10.9 / (1+i);\nvar p := 0.000000001 t + (1+i);\n"
       "var vx := (a+b) cos(p) - b cos((1+a/b)p);\nvar vy := (a+b) sin(p) - b sin((1+a/b)p);\n"
       "return [vx, vy]",
       278, true},
      {"var a := 0.1;\nvar b := 0.5;\nvar p := 0.000000001 t + (1+i);\n"
       "var vx := (a+b) cos(p) - b cos((1+a/b)p);\nvar vy := (a+b) sin(p) - b sin((1+a/b)p);\n"
       "return [vx, vy]",
       278, true},
      {"var a := 0.5 * (i/(n-1));\nvar b := -0.5  * (i/(n-1));\nvar p := 0.000000001 t + (1+i);\n"
       "var vx := (a - b) cos(p) + b cos (p (a-b) / b);\nvar vy := (a - b) sin(p) + b sin(p (a-b) / b);\n"
       "return [vx, vy]",
       278, false},
      {"var k := 3 / 7;\nvar p := 0.00000001 t + i;\nreturn [\n  cos(k p) cos(p),\n  sin(k p) sin(p)\n]",
       12, true},
      {"var k := 2 / 1;\nvar p := 0.00000001 t + i;\nreturn [\n  cos(k p) cos(p),\n  sin(k p) sin(p)\n]",
       12, true},
      {"var k := 2 / 1;\nvar p := 0.000000001 t + (1+i);\nreturn [\n  cos(k p) cos(p),\n  cos(k p) sin(p)\n]",
       48, true},
      {"var r := random(0, 1);\nvar p := random(0, 2 * pi);\nreturn [r cos(p), r sin(p)]",
       32, true},
  };

  for(const auto& [expr, sz, finite] : presets)
  {
    INFO(expr);
    arraygen_harness h;
    h.size(sz);
    h.set(expr);
    h.run(make_tick(1000000, 1001000, 0.25));

    const auto l = as_list(h.out.last());
    REQUIRE((int)l.size() == sz);
    for(auto& v : l)
    {
      INFO("element type: " << (int)v.get_type());
      REQUIRE(v.get_type() == ossia::val_type::VEC2F);
      auto& p = *v.target<ossia::vec2f>();
      if(finite)
      {
        CHECK(std::isfinite(p[0]));
        CHECK(std::isfinite(p[1]));
      }
    }
  }
}

TEST_CASE("Arraygen: the Sample circle preset stays inside the unit disc", "[fx][exprtk][arraygen][presets]")
{
  arraygen_harness h;
  h.size(32);
  h.set("var r := random(0, 1);\nvar p := random(0, 2 * pi);\nreturn [r cos(p), r sin(p)]");
  h.run();

  const auto l = as_list(h.out.last());
  REQUIRE(l.size() == 32);
  bool all_identical = true;
  auto first = as_vec2(l[0]);
  for(auto& v : l)
  {
    auto p = as_vec2(v);
    CHECK(std::hypot(p[0], p[1]) <= 1.0001f);
    if(p[0] != first[0])
      all_identical = false;
  }
  CHECK_FALSE(all_identical);
}

TEST_CASE("Arraygen: changing the size takes effect", "[fx][exprtk][arraygen]")
{
  arraygen_harness h;
  h.set("i");

  h.size(4);
  h.run();
  CHECK(as_list(h.out.last()).size() == 4);

  h.size(9);
  h.run();
  {
    const auto l = as_list(h.out.last());
    REQUIRE(l.size() == 9);
    CHECK(as_float(l[8]) == Approx(8.));
  }

  h.size(2);
  h.run();
  CHECK(as_list(h.out.last()).size() == 2);
}

TEST_CASE("Arraygen: changing the expression takes effect", "[fx][exprtk][arraygen]")
{
  arraygen_harness h;
  h.size(3);

  h.set("i");
  h.run();
  CHECK(as_float(as_list(h.out.last())[2]) == Approx(2.));

  h.set("i * 100");
  h.run();
  CHECK(as_float(as_list(h.out.last())[2]) == Approx(200.));

  h.set("return [i, 0]");
  h.run();
  CHECK(as_vec2(as_list(h.out.last())[2])[0] == Approx(2.f));

  h.set("i + 1");
  h.run();
  CHECK(as_float(as_list(h.out.last())[2]) == Approx(3.));
}

TEST_CASE("Arraygen: repeatedly re-setting the same expression stays stable", "[fx][exprtk][arraygen]")
{
  arraygen_harness h;
  h.size(8);
  h.set("cos(i) + t * 0");
  for(int i = 0; i < 200; i++)
  {
    h.run(i * 1000, i / 200.);
    const auto l = as_list(h.out.last());
    REQUIRE(l.size() == 8);
    CHECK(as_float(l[3]) == Approx(std::cos(3.)));
  }
}

TEST_CASE("Arraygen: degenerate sizes", "[fx][exprtk][arraygen]")
{
  {
    arraygen_harness h;
    h.size(0);
    h.set("i");
    h.run();
    CHECK(as_list(h.out.last()).empty());
  }
  {
    arraygen_harness h;
    h.size(1);
    h.set("i + 5");
    h.run();
    const auto l = as_list(h.out.last());
    REQUIRE(l.size() == 1);
    CHECK(as_float(l[0]) == Approx(5.));
  }
}

TEST_CASE("Arraygen: a size of one still returns the whole value", "[fx][exprtk][arraygen][return]")
{
  // A single element used to collapse to one float (the unset `po`), losing
  // whatever the expression returned, while sizes >= 2 worked.
  {
    arraygen_harness h;
    h.size(1);
    h.set("return [10, 20]");
    h.run();
    const auto l = as_list(h.out.last());
    REQUIRE(l.size() == 1);
    auto p = as_vec2(l[0]);
    CHECK(p[0] == Approx(10.f));
    CHECK(p[1] == Approx(20.f));
  }
  {
    arraygen_harness h;
    h.size(1);
    h.set("return [1, 2, 3, 4, 5]");
    h.run();
    const auto l = as_list(h.out.last());
    REQUIRE(l.size() == 1);
    const auto inner = as_list(l[0]);
    REQUIRE(inner.size() == 5);
    CHECK(as_float(inner[4]) == Approx(5.));
  }
  {
    // ... and the same expression at size 2 gives the same per-element value.
    arraygen_harness h;
    h.size(2);
    h.set("return [10, 20]");
    h.run();
    const auto l = as_list(h.out.last());
    REQUIRE(l.size() == 2);
    CHECK(as_vec2(l[0])[1] == Approx(20.f));
    CHECK(as_vec2(l[1])[1] == Approx(20.f));
  }
}

TEST_CASE("Arraygen: an invalid expression produces no output", "[fx][exprtk][arraygen][error]")
{
  arraygen_harness h;
  h.size(4);
  h.set("return [");
  h.run();
  CHECK(h.out.values.empty());

  h.set("i");
  h.run();
  CHECK(as_list(h.out.last()).size() == 4);
}

TEST_CASE("Arraygen: t and pos are visible to every element", "[fx][exprtk][arraygen]")
{
  arraygen_harness h;
  h.size(3);
  h.set("t + pos");
  h.run(make_tick(1000, 2000, 0.5));

  const auto l = as_list(h.out.last());
  REQUIRE(l.size() == 3);
  for(auto& v : l)
    CHECK(as_float(v) == Approx(1000.5));
}

// ===========================================================================
// Expression Audio Generator
// ===========================================================================

namespace
{
struct audiogen_harness
{
  Nodes::MathAudioGenerator::Node node;
  std::vector<double> l, r;
  double* chans[2]{};

  explicit audiogen_harness(int frames = 64)
      : l(frames, 0.)
      , r(frames, 0.)
  {
    chans[0] = l.data();
    chans[1] = r.data();
    node.outputs.audio.request_channels = [](int) {};
    node.prepare(halp::setup{
        .input_channels = 0, .output_channels = 2, .frames = frames, .rate = 44100.});
    node.outputs.audio.samples = chans;
    node.outputs.audio.channels = 2;
  }

  void set(const std::string& e) { node.inputs.expr.value = e; }
  void run(int frames)
  {
    halp::tick_flicks tk{};
    tk.frames = frames;
    node(tk);
  }
};
}

TEST_CASE("Expression Audio Generator: a constant expression", "[fx][exprtk][audiogen]")
{
  audiogen_harness h;
  h.set("out[0] := 0.5; out[1] := -0.5;");
  h.run(64);

  for(int i = 0; i < 64; i++)
  {
    CHECK(h.l[i] == Approx(0.5));
    CHECK(h.r[i] == Approx(-0.5));
  }
}

TEST_CASE("Expression Audio Generator: t counts frames inside the buffer", "[fx][exprtk][audiogen]")
{
  audiogen_harness h;
  h.set("out[0] := t; out[1] := t;");
  h.run(16);
  for(int i = 0; i < 16; i++)
    CHECK(h.l[i] == Approx((double)i));
}

TEST_CASE("Expression Audio Generator: fs is the sample rate", "[fx][exprtk][audiogen]")
{
  audiogen_harness h;
  h.set("out[0] := fs; out[1] := fs;");
  h.run(4);
  CHECK(h.l[0] == Approx(44100.));
}

TEST_CASE("Expression Audio Generator: the Sine preset oscillates", "[fx][exprtk][audiogen][presets]")
{
  audiogen_harness h{4096};
  h.set("var phi := 2 * pi * (20 + a * 500) / fs;\nm1[0] += phi;\n\n"
        "out[0] := b * cos(m1[0]);\nout[1] := b * cos(m1[0]);\n");
  h.node.inputs.a.value = 0.5f;
  h.node.inputs.b.value = 0.5f;
  h.run(4096);

  double mn = 1e9, mx = -1e9;
  for(auto v : h.l)
  {
    mn = std::min(mn, v);
    mx = std::max(mx, v);
    CHECK(std::abs(v) <= 0.5001);
  }
  CHECK(mx > 0.45);
  CHECK(mn < -0.45);
}

TEST_CASE("Expression Audio Generator: m1 persists across buffers", "[fx][exprtk][audiogen]")
{
  audiogen_harness h;
  h.set("m1[0] += 1; out[0] := m1[0]; out[1] := m1[0];");

  h.run(8);
  CHECK(h.l[7] == Approx(8.));
  h.run(8);
  CHECK(h.l[0] == Approx(9.));
  CHECK(h.l[7] == Approx(16.));
}

TEST_CASE("Expression Audio Generator: the Square preset is bounded", "[fx][exprtk][audiogen][presets]")
{
  audiogen_harness h{1024};
  h.set("\nvar phi := 2 * pi * (20 + a * 500) / fs;\n\nm1[0] += phi;\n\n"
        "var f := cos(m1[0]) > 0 ? b : -b;\nout[0] := f;\nout[1] := f;");
  h.node.inputs.a.value = 0.46f;
  h.node.inputs.b.value = 0.5f;
  h.run(1024);

  for(auto v : h.l)
    CHECK(std::abs(std::abs(v) - 0.5) < 1e-9);
}

TEST_CASE("Expression Audio Generator: an invalid expression leaves the buffer alone", "[fx][exprtk][audiogen][error]")
{
  audiogen_harness h;
  h.set("out[0] := ");
  h.run(16);
  for(int i = 0; i < 16; i++)
    CHECK(h.l[i] == 0.);
}
