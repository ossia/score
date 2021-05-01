#pragma once
#include <score/plugins/UuidKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

SCORE_SERALIZE_DATASTREAM_DEFINE_T(template <typename T>, UuidKey<T>)
