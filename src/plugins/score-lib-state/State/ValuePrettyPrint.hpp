#pragma once
#include <ossia/network/value/value.hpp>

#include <score_lib_state_export.h>

#include <string>

namespace State
{
/**
 * @brief Layout knobs for prettyPrintValue.
 */
struct PrettyPrintOptions
{
  //! Spaces added per nesting level.
  int indent{2};
  //! A container whose elements are all scalars is kept on one line up to this
  //! many elements; past it, it is wrapped elementsPerLine per line.
  int maxInlineElements{16};
  //! Elements per line when wrapping a long container of scalars.
  int elementsPerLine{8};
};

/**
 * @brief Appends a multi-line, indented rendering of v to out.
 *
 * The scalar tokens are those of fmt::format("{}", ossia::value) so that the
 * output only differs from the single-line form by its layout. The rules:
 *
 *  - a scalar is written as-is;
 *  - a list or map whose elements are all scalars stays on one line:
 *        list: [int: 0, int: 1, int: 2]
 *    unless it is long, in which case it is wrapped, a few elements per line;
 *  - a list or map with at least one nested container puts each element on its
 *    own line, indented one level deeper:
 *        list: [
 *          list: [int: 0, int: 1, int: 2],
 *          list: [int: 3, int: 4, int: 5]
 *        ]
 *    so that a matrix reads row by row rather than one number per line.
 *
 * Nesting depth is unbounded. Appends to `out` without clearing it and does not
 * allocate beyond `out`'s own growth; reuse the same string across calls to
 * amortize it.
 *
 * @param depth Nesting level of v itself; continuation lines are indented
 *        relative to it.
 */
SCORE_LIB_STATE_EXPORT
void prettyPrintValue(
    std::string& out, const ossia::value& v, int depth = 0,
    const PrettyPrintOptions& opts = {});

//! Single-line rendering of v, identical to fmt::format("{}", v), appended to out.
SCORE_LIB_STATE_EXPORT
void printValue(std::string& out, const ossia::value& v);
}
