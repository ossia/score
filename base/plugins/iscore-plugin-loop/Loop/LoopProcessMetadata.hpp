#pragma once
#include <Process/ProcessMetadata.hpp>
#include <QString>
#include <iscore_plugin_loop_export.h>

namespace Loop
{
class ProcessModel;
}

PROCESS_METADATA(
        ISCORE_PLUGIN_LOOP_EXPORT,
        Loop::ProcessModel,
        "995d41a8-0f10-4152-971d-e4c033579a02",
        "Loop",
        "Loop"
        )


UNDO_NAME_METADATA(EMPTY_MACRO, Loop::ProcessModel, "Loop")
