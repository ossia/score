// ossia::math_expression — the ExprTK wrapper every "expression" process in
// score is built on (Micromap, Expression Value Filter / Generator, Arraymap,
// Arraygen, Expression Audio Filter / Generator).
//
// The wrapper is thin but carries three pieces of real logic that the nodes
// depend on and that are easy to get subtly wrong:
//   * result(): ExprTK signals "the expression ended with `return [...]`" by
//     evaluating to NaN and filling a results context. Telling that apart from
//     an expression that genuinely computed a NaN is this class's job.
//   * has_variable(): a purely lexical query the nodes use to pick a code path
//     (array vs scalar). It must not depend on the compiled state.
//   * add_vector()/rebase_vector(): ExprTK views onto std::vectors owned by the
//     node, which are re-pointed as the input size changes.
#include <ossia/math/math_expression.hpp>
#include <ossia/network/value/value.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <vector>

using Catch::Approx;

namespace
{
float as_float(const ossia::value& v)
{
  REQUIRE(v.get_type() == ossia::val_type::FLOAT);
  return *v.target<float>();
}

std::vector<ossia::value> as_list(const ossia::value& v)
{
  REQUIRE(v.get_type() == ossia::val_type::LIST);
  return *v.target<std::vector<ossia::value>>();
}

// The symbol environment of the "Expression Value Generator" node, which is the
// richest of the value-domain ones: this is what user expressions see.
struct generator_env
{
  double t{}, dt{}, pos{};
  double a{}, b{}, c{};
  double m1{}, m2{}, m3{};
  double po{};
  std::vector<double> pov = std::vector<double>(16, 0.);

  ossia::math_expression expr;

  generator_env()
  {
    expr.add_vector("pov", pov);
    expr.add_variable("po", po);
    expr.add_variable("t", t);
    expr.add_variable("dt", dt);
    expr.add_variable("pos", pos);
    expr.add_variable("a", a);
    expr.add_variable("b", b);
    expr.add_variable("c", c);
    expr.add_variable("m1", m1);
    expr.add_variable("m2", m2);
    expr.add_variable("m3", m3);
    expr.add_constants();
    expr.register_symbol_table();
  }

  ossia::value eval(const std::string& e)
  {
    REQUIRE(expr.set_expression(e));
    return expr.result();
  }
};
}

// ---------------------------------------------------------------------------
// Scalars
// ---------------------------------------------------------------------------

TEST_CASE("math_expression: scalar arithmetic", "[exprtk][math_expression]")
{
  generator_env env;
  CHECK(as_float(env.eval("1 + 2 * 3")) == Approx(7.));
  CHECK(as_float(env.eval("2 ^ 10")) == Approx(1024.));
  CHECK(as_float(env.eval("10 % 3")) == Approx(1.));
  CHECK(as_float(env.eval("-4 + 1")) == Approx(-3.));
}

TEST_CASE("math_expression: variables are read at evaluation time", "[exprtk][math_expression]")
{
  generator_env env;
  env.a = 2.;
  REQUIRE(env.expr.set_expression("a * 10"));
  CHECK(as_float(env.expr.result()) == Approx(20.));

  // No recompilation: the same compiled expression sees the new value.
  env.a = 3.5;
  CHECK(as_float(env.expr.result()) == Approx(35.));
}

TEST_CASE("math_expression: implicit multiplication as used by the presets", "[exprtk][math_expression]")
{
  // Every shipped preset relies on this ExprTK feature ("100 a x", "0.1noise(...)")
  generator_env env;
  env.a = 0.5;
  env.t = 2.;
  CHECK(as_float(env.eval("100 a")) == Approx(50.));
  CHECK(as_float(env.eval("2 pi")) == Approx(2. * 3.141592653589793));
  CHECK(as_float(env.eval("3 t")) == Approx(6.));
}

TEST_CASE("math_expression: constants are registered", "[exprtk][math_expression]")
{
  generator_env env;
  CHECK(as_float(env.eval("pi")) == Approx(3.14159265).epsilon(1e-6));
  CHECK(as_float(env.eval("epsilon")) > 0.f);
}

TEST_CASE("math_expression: statement sequences return the last statement", "[exprtk][math_expression]")
{
  generator_env env;
  CHECK(as_float(env.eval("var x := 3; var y := 4; sqrt(x*x + y*y)")) == Approx(5.));
}

TEST_CASE("math_expression: for loops", "[exprtk][math_expression]")
{
  generator_env env;
  CHECK(
      as_float(env.eval("var acc := 0; for(var i := 0; i < 5; i += 1) { acc += i; }; acc"))
      == Approx(10.));
}

TEST_CASE("math_expression: memory variables persist across evaluations", "[exprtk][math_expression]")
{
  // The "Increment-on-press" / "Logistic" presets rely on m1..m3 keeping their
  // value from one tick to the next.
  generator_env env;
  REQUIRE(env.expr.set_expression("m1 := m1 + 1"));
  CHECK(as_float(env.expr.result()) == Approx(1.));
  CHECK(as_float(env.expr.result()) == Approx(2.));
  CHECK(as_float(env.expr.result()) == Approx(3.));
  CHECK(env.m1 == Approx(3.));
}

TEST_CASE("math_expression: set_expression with unchanged text does not reset state", "[exprtk][math_expression]")
{
  generator_env env;
  const std::string e = "m1 += 1";
  REQUIRE(env.expr.set_expression(e));
  env.expr.result();
  env.expr.result();
  // Re-setting the identical text must be a no-op, not a recompilation.
  REQUIRE(env.expr.set_expression(e));
  CHECK(as_float(env.expr.result()) == Approx(3.));
}

// ---------------------------------------------------------------------------
// return [...] — the list/vec output path
// ---------------------------------------------------------------------------

TEST_CASE("math_expression: return of one element gives a one-element list", "[exprtk][math_expression][return]")
{
  generator_env env;
  const auto l = as_list(env.eval("return [ 123 ]"));
  REQUIRE(l.size() == 1);
  CHECK(as_float(l[0]) == Approx(123.));
}

TEST_CASE("math_expression: return of 2/3/4 elements gives vec2f/vec3f/vec4f", "[exprtk][math_expression][return]")
{
  generator_env env;
  {
    auto v = env.eval("return [1, 2]");
    REQUIRE(v.get_type() == ossia::val_type::VEC2F);
    auto& vec = *v.target<ossia::vec2f>();
    CHECK(vec[0] == Approx(1.f));
    CHECK(vec[1] == Approx(2.f));
  }
  {
    auto v = env.eval("return [1, 2, 3]");
    REQUIRE(v.get_type() == ossia::val_type::VEC3F);
    auto& vec = *v.target<ossia::vec3f>();
    CHECK(vec[2] == Approx(3.f));
  }
  {
    auto v = env.eval("return [1, 2, 3, 4]");
    REQUIRE(v.get_type() == ossia::val_type::VEC4F);
    auto& vec = *v.target<ossia::vec4f>();
    CHECK(vec[3] == Approx(4.f));
  }
}

TEST_CASE("math_expression: return of 5+ elements gives a list", "[exprtk][math_expression][return]")
{
  generator_env env;
  const auto l = as_list(env.eval("return [1, 2, 3, 4, 5]"));
  REQUIRE(l.size() == 5);
  CHECK(as_float(l[4]) == Approx(5.));
}

TEST_CASE("math_expression: return reads registered variables", "[exprtk][math_expression][return]")
{
  generator_env env;
  env.pos = 0.25;
  env.a = 7.;
  const auto l = as_list(env.eval("return [ pos ]"));
  REQUIRE(l.size() == 1);
  CHECK(as_float(l[0]) == Approx(0.25));

  auto v = env.eval("return [ pos, a ]");
  REQUIRE(v.get_type() == ossia::val_type::VEC2F);
  CHECK((*v.target<ossia::vec2f>())[1] == Approx(7.f));
}

TEST_CASE("math_expression: a local var followed by return of a registered variable", "[exprtk][math_expression][return]")
{
  // Reported regression: `var rrr := pos; return [ pos ]` had to behave exactly
  // like `return [ pos ]`.
  generator_env env;
  env.pos = 0.75;

  const auto a = as_list(env.eval("return [ pos ]"));
  const auto b = as_list(env.eval("var rrr := pos; return [ pos ]"));
  const auto c = as_list(env.eval("var rrr := pos; return [ rrr ]"));

  REQUIRE(a.size() == 1);
  REQUIRE(b.size() == 1);
  REQUIRE(c.size() == 1);
  CHECK(as_float(a[0]) == Approx(0.75));
  CHECK(as_float(b[0]) == Approx(0.75));
  CHECK(as_float(c[0]) == Approx(0.75));
}

TEST_CASE("math_expression: return is re-evaluated every call", "[exprtk][math_expression][return]")
{
  generator_env env;
  REQUIRE(env.expr.set_expression("return [ t, t * 2 ]"));

  env.t = 1.;
  auto v1 = env.expr.result();
  env.t = 4.;
  auto v2 = env.expr.result();

  REQUIRE(v1.get_type() == ossia::val_type::VEC2F);
  REQUIRE(v2.get_type() == ossia::val_type::VEC2F);
  CHECK((*v1.target<ossia::vec2f>())[1] == Approx(2.f));
  CHECK((*v2.target<ossia::vec2f>())[1] == Approx(8.f));
}

TEST_CASE("math_expression: a NaN scalar result is not mistaken for a return", "[exprtk][math_expression][return]")
{
  // ExprTK marks "an explicit return happened" by evaluating to NaN *and*
  // filling the results context. An expression that merely computes a NaN
  // leaves the results context empty and must still be reported as a number.
  generator_env env;

  auto v = env.eval("sqrt(-1)");
  REQUIRE(v.get_type() == ossia::val_type::FLOAT);
  CHECK(std::isnan(*v.target<float>()));

  // ... and the node must not be left thinking the expression returned a list.
  CHECK(env.eval("0/0").get_type() == ossia::val_type::FLOAT);

  // A well-defined value still comes out as a float.
  CHECK(as_float(env.eval("1/0")) == std::numeric_limits<float>::infinity());
}

TEST_CASE("math_expression: a conditional return falls back to the scalar value", "[exprtk][math_expression][return]")
{
  generator_env env;
  REQUIRE(env.expr.set_expression("if(pos > 0.5) { return [1, 2]; }; 42"));

  env.pos = 0.;
  CHECK(as_float(env.expr.result()) == Approx(42.));

  env.pos = 1.;
  auto v = env.expr.result();
  REQUIRE(v.get_type() == ossia::val_type::VEC2F);
  CHECK((*v.target<ossia::vec2f>())[0] == Approx(1.f));
}

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

TEST_CASE("math_expression: a syntax error is reported and does not compile", "[exprtk][math_expression][error]")
{
  generator_env env;
  CHECK_FALSE(env.expr.set_expression("1 +"));
  CHECK_FALSE(env.expr.valid());
  CHECK_FALSE(env.expr.error().empty());
}

TEST_CASE("math_expression: an unknown symbol does not compile", "[exprtk][math_expression][error]")
{
  generator_env env;
  CHECK_FALSE(env.expr.set_expression("zorglub * 2"));
  CHECK_FALSE(env.expr.valid());
}

TEST_CASE("math_expression: recovering from an invalid expression", "[exprtk][math_expression][error]")
{
  generator_env env;
  REQUIRE_FALSE(env.expr.set_expression("1 +"));
  REQUIRE(env.expr.set_expression("1 + 1"));
  CHECK(env.expr.valid());
  CHECK(as_float(env.expr.result()) == Approx(2.));
  // The error belongs to the expression, not to the thread's last parse.
  CHECK(env.expr.error().empty());
}

TEST_CASE("math_expression: an error on one expression does not leak into another", "[exprtk][math_expression][error]")
{
  generator_env good, bad;
  REQUIRE(good.expr.set_expression("1 + 1"));
  REQUIRE_FALSE(bad.expr.set_expression("1 +"));

  CHECK(good.expr.error().empty());
  CHECK_FALSE(bad.expr.error().empty());
}

// ---------------------------------------------------------------------------
// has_variable — the array-vs-scalar switch of Micromap / Expression Value Filter
// ---------------------------------------------------------------------------

TEST_CASE("math_expression: has_variable finds a used variable", "[exprtk][math_expression][has_variable]")
{
  generator_env env;
  REQUIRE(env.expr.set_expression("a + b"));
  CHECK(env.expr.has_variable("a"));
  CHECK(env.expr.has_variable("b"));
  CHECK_FALSE(env.expr.has_variable("c"));
}

TEST_CASE("math_expression: has_variable is not fooled by longer identifiers", "[exprtk][math_expression][has_variable]")
{
  std::vector<double> xv(4, 1.), pxv(4, 1.);
  ossia::math_expression e;
  double po{};
  e.add_vector("xv", xv);
  e.add_vector("pxv", pxv);
  e.add_variable("po", po);
  e.add_constants();
  e.register_symbol_table();

  REQUIRE(e.set_expression("pxv[0] * 2"));
  CHECK(e.has_variable("pxv"));
  CHECK_FALSE(e.has_variable("xv"));

  REQUIRE(e.set_expression("xv[0] + pxv[1]"));
  CHECK(e.has_variable("xv"));
  CHECK(e.has_variable("pxv"));
}

TEST_CASE("math_expression: has_variable answers repeated queries consistently", "[exprtk][math_expression][has_variable]")
{
  // The answer is cached internally; the cache must not corrupt later answers
  // (a "not used" answer for one name used to poison the lookup for others).
  generator_env env;
  REQUIRE(env.expr.set_expression("m3 + t"));
  for(int i = 0; i < 3; i++)
  {
    CHECK_FALSE(env.expr.has_variable("m1"));
    CHECK_FALSE(env.expr.has_variable("m2"));
    CHECK(env.expr.has_variable("m3"));
    CHECK(env.expr.has_variable("t"));
    CHECK_FALSE(env.expr.has_variable("pos"));
  }
}

TEST_CASE("math_expression: has_variable is purely lexical", "[exprtk][math_expression][has_variable]")
{
  // The nodes call it to pick a code path; it must answer for the *text*,
  // whether or not the expression is currently compilable.
  std::vector<double> xv(1, 0.);
  ossia::math_expression e;
  e.add_vector("xv", xv);
  e.add_constants();
  e.register_symbol_table();

  // xv has a single element: xv[2] cannot compile.
  CHECK_FALSE(e.set_expression("xv[2]"));
  CHECK(e.has_variable("xv"));
}

// ---------------------------------------------------------------------------
// Vectors
// ---------------------------------------------------------------------------

TEST_CASE("math_expression: vector element access", "[exprtk][math_expression][vector]")
{
  std::vector<double> xv{1., 2., 3.};
  ossia::math_expression e;
  e.add_vector("xv", xv);
  e.add_constants();
  e.register_symbol_table();

  REQUIRE(e.set_expression("xv[0] + xv[1] + xv[2]"));
  CHECK(as_float(e.result()) == Approx(6.));

  xv[0] = 10.;
  CHECK(as_float(e.result()) == Approx(15.));
}

TEST_CASE("math_expression: a vector's size is visible to the expression", "[exprtk][math_expression][vector]")
{
  std::vector<double> xv{1., 2., 3.};
  ossia::math_expression e;
  e.add_vector("xv", xv);
  e.add_constants();
  e.register_symbol_table();

  REQUIRE(e.set_expression("xv[]"));
  CHECK(as_float(e.result()) == Approx(3.));
}

TEST_CASE("math_expression: rebase_vector re-points and resizes the view", "[exprtk][math_expression][vector]")
{
  std::vector<double> xv{1., 2., 3., 4.};
  ossia::math_expression e;
  e.add_vector("xv", xv);
  e.add_constants();
  e.register_symbol_table();

  REQUIRE(e.set_expression("var acc := 0; for(var i := 0; i < xv[]; i += 1) { acc += xv[i]; }; acc"));
  CHECK(as_float(e.result()) == Approx(10.));

  // The node shrinks its buffer and re-points the view: same expression, new size.
  xv.assign({5., 6.});
  e.rebase_vector("xv", xv);
  REQUIRE(e.recompile());
  CHECK(as_float(e.result()) == Approx(11.));

  // ... and grows it again.
  xv.assign({1., 1., 1., 1., 1., 1.});
  e.rebase_vector("xv", xv);
  REQUIRE(e.recompile());
  CHECK(as_float(e.result()) == Approx(6.));
}

TEST_CASE("math_expression: writing into a vector writes through to the node's buffer", "[exprtk][math_expression][vector]")
{
  std::vector<double> out(2, 0.);
  ossia::math_expression e;
  e.add_vector("out", out);
  e.add_constants();
  e.register_symbol_table();

  REQUIRE(e.set_expression("out[0] := 1; out[1] := 2;"));
  e.value();
  CHECK(out[0] == Approx(1.));
  CHECK(out[1] == Approx(2.));
}

TEST_CASE("math_expression: returning a vector variable", "[exprtk][math_expression][vector][return]")
{
  generator_env env;
  const auto outer = as_list(env.eval("var v[3] := {7, 8, 9}; return [v]"));
  REQUIRE(outer.size() == 1);
  const auto inner = as_list(outer[0]);
  REQUIRE(inner.size() == 3);
  CHECK(as_float(inner[0]) == Approx(7.));
  CHECK(as_float(inner[2]) == Approx(9.));
}

// ---------------------------------------------------------------------------
// The score-specific function library
// ---------------------------------------------------------------------------

TEST_CASE("math_expression: map() rescales between two ranges", "[exprtk][math_expression][functions]")
{
  generator_env env;
  CHECK(as_float(env.eval("map(0.5, 0, 1, 10, 20)")) == Approx(15.));
  CHECK(as_float(env.eval("map(0, 0, 1, 10, 20)")) == Approx(10.));
  CHECK(as_float(env.eval("map(2, 0, 1, 10, 20)")) == Approx(30.));
  // Degenerate input range: no division by zero.
  CHECK(as_float(env.eval("map(5, 1, 1, 10, 20)")) == Approx(10.));
}

TEST_CASE("math_expression: norm() normalizes into 0-1", "[exprtk][math_expression][functions]")
{
  generator_env env;
  CHECK(as_float(env.eval("norm(5, 0, 10)")) == Approx(0.5));
  CHECK(as_float(env.eval("norm(5, 10, 0)")) == Approx(0.));  // degenerate
  CHECK(as_float(env.eval("norm(5, 3, 3)")) == Approx(0.));   // degenerate
}

TEST_CASE("math_expression: step()", "[exprtk][math_expression][functions]")
{
  generator_env env;
  CHECK(as_float(env.eval("step(0.2, 0.5)")) == Approx(0.));
  CHECK(as_float(env.eval("step(0.5, 0.5)")) == Approx(1.));
  CHECK(as_float(env.eval("step(0.9, 0.5)")) == Approx(1.));
}

TEST_CASE("math_expression: lerp()", "[exprtk][math_expression][functions]")
{
  generator_env env;
  CHECK(as_float(env.eval("lerp(0, 10, 20)")) == Approx(10.));
  CHECK(as_float(env.eval("lerp(1, 10, 20)")) == Approx(20.));
  CHECK(as_float(env.eval("lerp(0.25, 10, 20)")) == Approx(12.5));
}

TEST_CASE("math_expression: smoothstep() and smoothstep5()", "[exprtk][math_expression][functions]")
{
  generator_env env;
  CHECK(as_float(env.eval("smoothstep(-1, 0, 1)")) == Approx(0.));
  CHECK(as_float(env.eval("smoothstep(0.5, 0, 1)")) == Approx(0.5));
  CHECK(as_float(env.eval("smoothstep(2, 0, 1)")) == Approx(1.));
  CHECK(as_float(env.eval("smoothstep(1, 3, 3)")) == Approx(0.)); // degenerate

  CHECK(as_float(env.eval("smoothstep5(-1, 0, 1)")) == Approx(0.));
  CHECK(as_float(env.eval("smoothstep5(0.5, 0, 1)")) == Approx(0.5));
  CHECK(as_float(env.eval("smoothstep5(2, 0, 1)")) == Approx(1.));
  CHECK(as_float(env.eval("smoothstep5(1, 3, 3)")) == Approx(0.)); // degenerate
}

TEST_CASE("math_expression: bitwise helpers", "[exprtk][math_expression][functions]")
{
  generator_env env;
  CHECK(as_float(env.eval("bitwise_and(12, 10)")) == Approx(8.));
  CHECK(as_float(env.eval("bitwise_or(12, 10)")) == Approx(14.));
  CHECK(as_float(env.eval("bitwise_xor(12, 10)")) == Approx(6.));
  CHECK(as_float(env.eval("bitwise_shiftl(1, 4)")) == Approx(16.));
  CHECK(as_float(env.eval("bitwise_shiftr(16, 4)")) == Approx(1.));
  CHECK(as_float(env.eval("bitwise_not(0)")) == Approx(-1.));
}

TEST_CASE("math_expression: random() stays in range and is seedable", "[exprtk][math_expression][functions]")
{
  generator_env env;
  REQUIRE(env.expr.set_expression("random(2, 5)"));
  for(int i = 0; i < 64; i++)
  {
    const auto v = as_float(env.expr.result());
    CHECK(v >= 2.f);
    CHECK(v <= 5.f);
  }
  // Reversed bounds are accepted.
  CHECK(as_float(env.eval("random(5, 2)")) >= 2.f);
  // Degenerate bounds.
  CHECK(as_float(env.eval("random(3, 3)")) == Approx(3.));
}

TEST_CASE("math_expression: seed_random makes the sequence reproducible", "[exprtk][math_expression][functions]")
{
  auto draw = [](uint64_t s1, uint64_t s2) {
    generator_env env;
    env.expr.seed_random(s1, s2);
    REQUIRE(env.expr.set_expression("random(0, 1)"));
    std::vector<float> out;
    for(int i = 0; i < 8; i++)
      out.push_back(as_float(env.expr.result()));
    return out;
  };

  CHECK(draw(1, 2) == draw(1, 2));
  CHECK(draw(1, 2) != draw(3, 4));
}

TEST_CASE("math_expression: noise() is a normalized perlin octave", "[exprtk][math_expression][functions]")
{
  generator_env env;
  REQUIRE(env.expr.set_expression("noise(t * 0.1, 3, 0.5)"));
  for(int i = 0; i < 64; i++)
  {
    env.t = i;
    const auto v = as_float(env.expr.result());
    CHECK(v >= 0.f);
    CHECK(v <= 1.f);
  }
  // Same input, same output.
  env.t = 12.;
  CHECK(as_float(env.expr.result()) == Approx(as_float(env.expr.result())));
}

// ---------------------------------------------------------------------------
// The shipped presets, evaluated straight through math_expression
// ---------------------------------------------------------------------------

TEST_CASE("math_expression: shipped Expression Value Generator presets compile", "[exprtk][math_expression][presets]")
{
  const std::vector<std::string> presets{
      // Color Noise
      "var time := pos  * a * 10;\n"
      "var red := noise(time, b * 10, c); \n"
      "var green := noise(time + 1000, b * 10, c); \n"
      "var blue := noise(time + 100000, b * 10 , c); \n"
      "return [red, green, blue, 1]",
      // Dice
      "round(random(1, 6))",
      // Fast Noise
      "5 * (noise(pos * 100, 4, c) - 0.5) ",
      // Logistic
      "if(m1 == 0) {\n  m2 := 0.8;\n  m1 := 1;\n}\n\nvar r := 4 * a;\nm2 := r * m2 * (1 - m2);\nm2;",
      // Perlin Noise
      "noise(pos * a * 10, b * 10, c)  * 100",
      // Random Color
      "return [\n  random(0, a), \n  random(0, b),\n  random(0, c), \n  1\n]",
      // Soft Noise
      "noise(pos * 10, 3, 0.1)",
      // Weierstrass
      "var n:= 10;\nvar accum := 0;\nvar bb := round(b * 2 + c * 20 + 1);\n"
      "for (var i := 0; i < n; i += 1)  {\n  accum += (a ^ n) cos(t pi bb^n);\n};\naccum",
      // Vertical scroll
      "return [0, t / 1e6] ",
      // XY / XYZ
      "return [a, b];",
      "return [a, b, c];",
  };

  for(const auto& p : presets)
  {
    generator_env env;
    env.a = 0.5;
    env.b = 0.5;
    env.c = 0.5;
    env.t = 1000.;
    env.pos = 0.3;
    INFO(p);
    REQUIRE(env.expr.set_expression(p));
    CHECK(env.expr.valid());
    // and evaluates to something that is not "nothing"
    CHECK(env.expr.result().get_type() != ossia::val_type::NONE);
  }
}

TEST_CASE("math_expression: shipped Arraygen presets compile and return points", "[exprtk][math_expression][presets]")
{
  const std::vector<std::string> presets{
      "var a := 100.1 / (1+i);\nvar b := 10.9 / (1+i);\nvar p := 0.000000001 t + (1+i);\n"
      "var vx := (a+b) cos(p) - b cos((1+a/b)p);\nvar vy := (a+b) sin(p) - b sin((1+a/b)p);\n"
      "return [vx, vy]",
      "var k := 3 / 7;\nvar p := 0.00000001 t + i;\nreturn [\n  cos(k p) cos(p),\n  sin(k p) sin(p)\n]",
      "var r := random(0, 1);\nvar p := random(0, 2 * pi);\nreturn [r cos(p), r sin(p)]",
  };

  for(const auto& p : presets)
  {
    double i{2.}, n{12.}, po{}, t{1000.}, dt{}, pos{0.5};
    ossia::math_expression e;
    e.add_variable("i", i);
    e.add_variable("n", n);
    e.add_variable("po", po);
    e.add_variable("t", t);
    e.add_variable("dt", dt);
    e.add_variable("pos", pos);
    e.add_constants();
    e.register_symbol_table();

    INFO(p);
    REQUIRE(e.set_expression(p));
    auto v = e.result();
    CHECK(v.get_type() == ossia::val_type::VEC2F);
  }
}
