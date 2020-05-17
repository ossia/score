#pragma once
#include <ossia/dataflow/nodes/media.hpp>

#include <verdigris>

namespace Media
{
using audio_handle = ossia::audio_handle;
using audio_array = ossia::audio_array;
using audio_sample = ossia::audio_sample;
}

Q_DECLARE_METATYPE(Media::audio_handle)
W_REGISTER_ARGTYPE(Media::audio_handle)
