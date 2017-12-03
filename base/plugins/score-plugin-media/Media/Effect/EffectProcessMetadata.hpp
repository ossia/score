#pragma once
#include <Process/ProcessMetadata.hpp>
#include <score_plugin_media_export.h>

namespace Media
{
namespace Effect
{
class ProcessModel;
}
}

PROCESS_METADATA(
        SCORE_PLUGIN_MEDIA_EXPORT,
        Media::Effect::ProcessModel,
        "d27bc0ed-a93e-434c-913d-ccab0b22b4e8",
        "Effects",
        "Effect chain",
        "Structure",
        {}
        )


UNDO_NAME_METADATA(EMPTY_MACRO, Media::Effect::ProcessModel, "Effects")
