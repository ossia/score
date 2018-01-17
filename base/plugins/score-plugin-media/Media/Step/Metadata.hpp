#pragma once
#include <Process/ProcessMetadata.hpp>
#include <score_plugin_media_export.h>

namespace Media
{
namespace Step
{
class Model;
}
}

PROCESS_METADATA(
        SCORE_PLUGIN_MEDIA_EXPORT,
        Media::Step::Model,
        "c953fa55-65f0-4b93-8bfc-54780250d2b8",
        "Step",
        "Step sequencer",
        "Control",
        {},
        Process::ProcessFlags::SupportsTemporal |
        Process::ProcessFlags::PutInNewSlot
        )


UNDO_NAME_METADATA(EMPTY_MACRO, Media::Step::Model, "Step sequencer")
