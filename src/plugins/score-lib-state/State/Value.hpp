#pragma once
#include <score/tools/std/Optional.hpp>

#include <ossia-qt/value_metatypes.hpp>
#include <ossia/network/value/value.hpp>

#include <score_lib_state_export.h>

class QDebug;

namespace State
{
using impulse = ossia::impulse;

using vec2f = ossia::vec2f;
using vec3f = ossia::vec3f;
using vec4f = ossia::vec4f;
using list_t = std::vector<ossia::value>;

using Value = ossia::value;

SCORE_LIB_STATE_EXPORT std::optional<ossia::value> parseValue(const std::string& str);
SCORE_LIB_STATE_EXPORT QDebug& operator<<(QDebug& s, const Value& m);
}
