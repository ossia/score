#pragma once
#include <score/serialization/DataStreamFwd.hpp>
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

SCORE_LIB_STATE_EXPORT std::optional<ossia::value>
parseValue(const std::string& str);
SCORE_LIB_STATE_EXPORT QDebug& operator<<(QDebug& s, const Value& m);
}
SCORE_SERIALIZE_DATASTREAM_DECLARE(SCORE_LIB_STATE_EXPORT, State::impulse);
SCORE_SERIALIZE_DATASTREAM_DECLARE(SCORE_LIB_STATE_EXPORT, State::vec2f);
SCORE_SERIALIZE_DATASTREAM_DECLARE(SCORE_LIB_STATE_EXPORT, State::vec3f);
SCORE_SERIALIZE_DATASTREAM_DECLARE(SCORE_LIB_STATE_EXPORT, State::vec4f);
SCORE_SERIALIZE_DATASTREAM_DECLARE(SCORE_LIB_STATE_EXPORT, State::list_t);
SCORE_SERIALIZE_DATASTREAM_DECLARE(SCORE_LIB_STATE_EXPORT, State::Value);
