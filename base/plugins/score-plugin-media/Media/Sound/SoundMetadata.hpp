#pragma once
#include <Process/ProcessMetadata.hpp>
#include <score_plugin_media_export.h>

namespace Media
{
namespace Sound
{
class ProcessModel;
}
}

PROCESS_METADATA(
        SCORE_PLUGIN_MEDIA_EXPORT,
        Media::Sound::ProcessModel,
        "63174570-d608-44bf-a9cb-e6f5a11f73cc",
        "Sound",
        "Sound file",
        "Audio",
        {},
        Process::ProcessFlags::SupportsTemporal
        )


UNDO_NAME_METADATA(EMPTY_MACRO, Media::Sound::ProcessModel, "Sound")
