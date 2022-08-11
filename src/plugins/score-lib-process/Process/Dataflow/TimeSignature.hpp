#pragma once
#include <score/serialization/DataStreamFwd.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/editor/scenario/time_signature.hpp>

#include <ossia-qt/time.hpp>

#include <string>
#include <utility>
#include <verdigris>

SCORE_SERIALIZE_DATASTREAM_DECLARE(, ossia::time_signature)

Q_DECLARE_METATYPE(std::optional<ossia::time_signature>)
W_REGISTER_ARGTYPE(std::optional<ossia::time_signature>)
