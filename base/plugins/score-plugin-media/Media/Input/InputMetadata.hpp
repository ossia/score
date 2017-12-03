#pragma once
#include <Process/ProcessMetadata.hpp>
#include <score_plugin_media_export.h>

namespace Media
{
namespace Input
{
class ProcessModel;
}
}

PROCESS_METADATA(
        SCORE_PLUGIN_MEDIA_EXPORT,
        Media::Input::ProcessModel,
        "fe6ec588-23c8-458c-ad47-7fdfe0e813cc",
        "Input",
        "Audio input",
        "Audio",
        {}
        )


UNDO_NAME_METADATA(EMPTY_MACRO, Media::Input::ProcessModel, "Input")
