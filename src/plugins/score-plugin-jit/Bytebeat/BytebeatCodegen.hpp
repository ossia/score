#pragma once
#include <string>
#include <string_view>

#include <score_plugin_jit_export.h>

namespace Jit::Bytebeat
{
// How the formula is evaluated.
// JavaScript: what the web players (dollchan, greggman) run: every value is a
// double, bitwise operators go through ToInt32, Math is available without prefix.
// C: viznut's original `int t; putchar(EXPR)`: 32-bit integers, wrapping.
enum class Language : int
{
  JavaScript = 0,
  C = 1
};

// How the value returned by the formula becomes a sample.
enum class OutputMode : int
{
  Bytebeat = 0,       // (v & 255) is an unsigned 8-bit sample
  SignedBytebeat = 1, // (v & 255) is a signed 8-bit sample
  Floatbeat = 2       // v is a sample in [-1, 1]
};

// Entry point of the compiled program.
// Fills `frames` samples of `left` and `right`, evaluating the formula once per
// distinct value in `t`. `state` is a zero-initialized buffer of the size the
// function returns when called with a null state; it holds the variables of the
// formula, which persist from one sample to the next as in the JS players.
using Function = int(
    void* state, double* left, double* right, const double* t, int frames, int mode);
inline constexpr const char* function_name = "score_bytebeat";

struct Generated
{
  std::string code;
  std::string error;
  int error_line{-1};
  int formulas{};
};

// One formula per line: several lines are mixed together.
// A line continues the previous formula when its brackets are not balanced, or
// when it starts or the previous one ends with a binary operator.
SCORE_PLUGIN_JIT_TEST_EXPORT
Generated generate(std::string_view source, Language lang);
}
